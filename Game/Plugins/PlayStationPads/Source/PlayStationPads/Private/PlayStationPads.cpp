// DualSense and DualShock 4 on Windows, read straight from HID.
//
// Unreal only speaks XInput on Windows, which PlayStation pads don't. Instead of asking players
// to install DS4Windows, we open the pads ourselves and hand Unreal the same gamepad events an
// Xbox pad would produce. Menus, the fight input and splitscreen don't know the difference.
//
// A background thread looks for pads every two seconds (so plugging in mid-match works) and
// each pad gets its own reader thread. The game thread only copies the newest state per frame.

#include "CoreMinimal.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "GenericPlatform/IInputInterface.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "IInputDevice.h"
#include "IInputDeviceModule.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"

#include <atomic>

#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include <setupapi.h>
#include <hidsdi.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlayStationPads, Log, All);

namespace PlayStationPads
{
	constexpr uint16 SonyVendor = 0x054C;
	constexpr int32 MaxPads = 4;
	constexpr float StickButtonThreshold = 0.5f;
	constexpr double FirstRepeat = 0.2;
	constexpr double NextRepeat = 0.1;

	enum class EKind : uint8 { DualSense, DualShock4 };

	bool KindOf(uint16 Product, EKind& Out)
	{
		switch (Product)
		{
		case 0x0CE6: // DualSense
		case 0x0DF2: // DualSense Edge
			Out = EKind::DualSense;
			return true;
		case 0x05C4: // DualShock 4
		case 0x09CC: // DualShock 4, second revision
		case 0x0BA0: // DualShock 4 USB wireless adapter
			Out = EKind::DualShock4;
			return true;
		default:
			return false;
		}
	}

	enum EButton : int32
	{
		Cross, Circle, Square, Triangle, L1, R1, L2, R2, Create, Options, L3, R3,
		DPadUp, DPadDown, DPadLeft, DPadRight, PS, Touchpad,
		LeftUp, LeftDown, LeftLeft, LeftRight, RightUp, RightDown, RightLeft, RightRight,
		ButtonCount
	};
	enum EAxis : int32 { LeftX, LeftY, RightX, RightY, LeftTrigger, RightTrigger, AxisCount };

	// Same keys an Xbox pad sends, by position: cross is "bottom face button" and so on.
	FName ButtonKey(int32 Button)
	{
		switch (Button)
		{
		case Cross: return FGamepadKeyNames::FaceButtonBottom;
		case Circle: return FGamepadKeyNames::FaceButtonRight;
		case Square: return FGamepadKeyNames::FaceButtonLeft;
		case Triangle: return FGamepadKeyNames::FaceButtonTop;
		case L1: return FGamepadKeyNames::LeftShoulder;
		case R1: return FGamepadKeyNames::RightShoulder;
		case L2: return FGamepadKeyNames::LeftTriggerThreshold;
		case R2: return FGamepadKeyNames::RightTriggerThreshold;
		case Create: return FGamepadKeyNames::SpecialLeft;
		case Options: return FGamepadKeyNames::SpecialRight;
		case L3: return FGamepadKeyNames::LeftThumb;
		case R3: return FGamepadKeyNames::RightThumb;
		case DPadUp: return FGamepadKeyNames::DPadUp;
		case DPadDown: return FGamepadKeyNames::DPadDown;
		case DPadLeft: return FGamepadKeyNames::DPadLeft;
		case DPadRight: return FGamepadKeyNames::DPadRight;
		case LeftUp: return FGamepadKeyNames::LeftStickUp;
		case LeftDown: return FGamepadKeyNames::LeftStickDown;
		case LeftLeft: return FGamepadKeyNames::LeftStickLeft;
		case LeftRight: return FGamepadKeyNames::LeftStickRight;
		case RightUp: return FGamepadKeyNames::RightStickUp;
		case RightDown: return FGamepadKeyNames::RightStickDown;
		case RightLeft: return FGamepadKeyNames::RightStickLeft;
		case RightRight: return FGamepadKeyNames::RightStickRight;
		default: return NAME_None; // PS and touchpad have no Xbox twin, nothing uses them yet
		}
	}

	FName AxisKey(int32 Axis)
	{
		switch (Axis)
		{
		case LeftX: return FGamepadKeyNames::LeftAnalogX;
		case LeftY: return FGamepadKeyNames::LeftAnalogY;
		case RightX: return FGamepadKeyNames::RightAnalogX;
		case RightY: return FGamepadKeyNames::RightAnalogY;
		case LeftTrigger: return FGamepadKeyNames::LeftTriggerAnalog;
		default: return FGamepadKeyNames::RightTriggerAnalog;
		}
	}

	struct FState
	{
		uint32 Buttons = 0;
		float Axis[AxisCount] = {};

		bool Has(int32 Button) const { return (Buttons & (1u << Button)) != 0; }
	};

	float Stick(uint8 V) { return FMath::Clamp((static_cast<float>(V) - 128.f) / 127.f, -1.f, 1.f); }

	// Every report boils down to the same bytes, they only sit at different offsets.
	void Decode(const uint8* Sticks, uint8 B0, uint8 B1, uint8 B2, uint8 L2Value, uint8 R2Value, FState& Out)
	{
		uint32 B = 0;
		auto Set = [&B](int32 Button, bool bOn) { if (bOn) B |= 1u << Button; };

		const uint8 Hat = B0 & 0x0F; // 0 = up, clockwise to 7 = up-left, 8 = nothing
		Set(DPadUp, Hat == 7 || Hat == 0 || Hat == 1);
		Set(DPadRight, Hat >= 1 && Hat <= 3);
		Set(DPadDown, Hat >= 3 && Hat <= 5);
		Set(DPadLeft, Hat >= 5 && Hat <= 7);
		Set(Square, (B0 & 0x10) != 0);
		Set(Cross, (B0 & 0x20) != 0);
		Set(Circle, (B0 & 0x40) != 0);
		Set(Triangle, (B0 & 0x80) != 0);
		Set(L1, (B1 & 0x01) != 0);
		Set(R1, (B1 & 0x02) != 0);
		Set(L2, (B1 & 0x04) != 0);
		Set(R2, (B1 & 0x08) != 0);
		Set(Create, (B1 & 0x10) != 0);
		Set(Options, (B1 & 0x20) != 0);
		Set(L3, (B1 & 0x40) != 0);
		Set(R3, (B1 & 0x80) != 0);
		Set(PS, (B2 & 0x01) != 0);
		Set(Touchpad, (B2 & 0x02) != 0);

		Out.Axis[LeftX] = Stick(Sticks[0]);
		Out.Axis[LeftY] = -Stick(Sticks[1]); // the pad counts down as positive, Unreal wants up
		Out.Axis[RightX] = Stick(Sticks[2]);
		Out.Axis[RightY] = -Stick(Sticks[3]);
		Out.Axis[LeftTrigger] = L2Value / 255.f;
		Out.Axis[RightTrigger] = R2Value / 255.f;

		// stick directions as buttons too, like the Xbox driver; menus navigate with them
		Set(LeftUp, Out.Axis[LeftY] > StickButtonThreshold);
		Set(LeftDown, Out.Axis[LeftY] < -StickButtonThreshold);
		Set(LeftLeft, Out.Axis[LeftX] < -StickButtonThreshold);
		Set(LeftRight, Out.Axis[LeftX] > StickButtonThreshold);
		Set(RightUp, Out.Axis[RightY] > StickButtonThreshold);
		Set(RightDown, Out.Axis[RightY] < -StickButtonThreshold);
		Set(RightLeft, Out.Axis[RightX] < -StickButtonThreshold);
		Set(RightRight, Out.Axis[RightX] > StickButtonThreshold);
		Out.Buttons = B;
	}

	bool Parse(EKind Kind, const uint8* R, int32 Length, FState& Out)
	{
		if (Length < 10) return false;
		if (Kind == EKind::DualSense)
		{
			// USB (report 0x01, 64 bytes) and Bluetooth full mode (0x31) share a layout,
			// Bluetooth just starts one byte later
			int32 Base = -1;
			if (R[0] == 0x01 && Length >= 64) Base = 1;
			else if (R[0] == 0x31 && Length >= 12) Base = 2;
			if (Base > 0)
			{
				const uint8* D = R + Base;
				Decode(D, D[7], D[8], D[9], D[4], D[5], Out);
				return true;
			}
		}
		// The short layout: DualShock 4 over USB, DualShock 4 Bluetooth (0x11, two bytes later)
		// and the DualSense in Bluetooth basic mode.
		int32 Base = -1;
		if (R[0] == 0x01) Base = 1;
		else if (R[0] == 0x11 && Length >= 12) Base = 3;
		if (Base < 0) return false;
		const uint8* D = R + Base;
		Decode(D, D[4], D[5], D[6], D[7], D[8], Out);
		return true;
	}

	// Blocks on one pad's reports and keeps the newest one around.
	class FReader final : public FRunnable
	{
	public:
		FReader(HANDLE InFile, EKind InKind, int32 InReportSize) : File(InFile), Kind(InKind), ReportSize(InReportSize) {}

		virtual uint32 Run() override
		{
			TArray<uint8> Report;
			Report.SetNumZeroed(ReportSize);
			OVERLAPPED Overlapped = {};
			Overlapped.hEvent = CreateEventW(nullptr, true, false, nullptr);
			while (!bStop)
			{
				ResetEvent(Overlapped.hEvent);
				DWORD Read = 0;
				if (!ReadFile(File, Report.GetData(), static_cast<DWORD>(ReportSize), nullptr, &Overlapped) && GetLastError() != ERROR_IO_PENDING)
				{
					bDead = true;
					break;
				}
				// wait for the report, looking up now and then in case we should stop
				DWORD Wait = WAIT_TIMEOUT;
				while ((Wait = WaitForSingleObject(Overlapped.hEvent, 100)) == WAIT_TIMEOUT && !bStop) {}
				if (Wait != WAIT_OBJECT_0)
				{
					CancelIoEx(File, &Overlapped);
					GetOverlappedResult(File, &Overlapped, &Read, true);
					break;
				}
				if (!GetOverlappedResult(File, &Overlapped, &Read, false))
				{
					bDead = true; // unplugged or out of battery
					break;
				}
				FState Parsed;
				if (Parse(Kind, Report.GetData(), static_cast<int32>(Read), Parsed))
				{
					FScopeLock Guard(&Lock);
					Latest = Parsed;
				}
			}
			CloseHandle(Overlapped.hEvent);
			return 0;
		}

		virtual void Stop() override { bStop = true; }

		FState Get() const
		{
			FScopeLock Guard(&Lock);
			return Latest;
		}

		std::atomic<bool> bDead { false };

	private:
		HANDLE File;
		EKind Kind;
		int32 ReportSize;
		std::atomic<bool> bStop { false };
		mutable FCriticalSection Lock;
		FState Latest;
	};

	struct FFoundPad
	{
		FString Path;
		HANDLE File = INVALID_HANDLE_VALUE;
		EKind Kind = EKind::DualSense;
		int32 ReportSize = 64;
	};

	// Looks for PlayStation pads that aren't open yet.
	class FScanner final : public FRunnable
	{
	public:
		virtual uint32 Run() override
		{
			while (!bStop)
			{
				Scan();
				for (int32 i = 0; i < 20 && !bStop; ++i) FPlatformProcess::Sleep(0.1f);
			}
			return 0;
		}

		virtual void Stop() override { bStop = true; }

		bool Take(FFoundPad& Out)
		{
			FScopeLock Guard(&Lock);
			if (Found.Num() == 0) return false;
			Out = Found[0];
			Found.RemoveAt(0);
			return true;
		}

		void Forget(const FString& Path)
		{
			FScopeLock Guard(&Lock);
			Known.Remove(Path);
		}

		~FScanner()
		{
			for (const FFoundPad& Pad : Found) CloseHandle(Pad.File);
		}

	private:
		void Scan()
		{
			GUID HidGuid;
			HidD_GetHidGuid(&HidGuid);
			HDEVINFO Info = SetupDiGetClassDevsW(&HidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
			if (Info == INVALID_HANDLE_VALUE) return;

			SP_DEVICE_INTERFACE_DATA Interface = {};
			Interface.cbSize = sizeof(Interface);
			for (DWORD Index = 0; !bStop && SetupDiEnumDeviceInterfaces(Info, nullptr, &HidGuid, Index, &Interface); ++Index)
			{
				DWORD Size = 0;
				SetupDiGetDeviceInterfaceDetailW(Info, &Interface, nullptr, 0, &Size, nullptr);
				if (Size == 0) continue;
				TArray<uint8> Buffer;
				Buffer.SetNumZeroed(Size);
				auto* Detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(Buffer.GetData());
				Detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
				if (!SetupDiGetDeviceInterfaceDetailW(Info, &Interface, Detail, Size, nullptr, nullptr)) continue;

				const FString Path(Detail->DevicePath);
				{
					FScopeLock Guard(&Lock);
					if (Known.Contains(Path) || Known.Num() >= MaxPads) continue;
				}
				FFoundPad Pad;
				if (!Open(Path, Pad)) continue;
				FScopeLock Guard(&Lock);
				Known.Add(Path);
				Found.Add(Pad);
			}
			SetupDiDestroyDeviceInfoList(Info);
		}

		static bool Open(const FString& Path, FFoundPad& Out)
		{
			// Query-only handle first: works on every HID device, even keyboards and mice.
			HANDLE Query = CreateFileW(*Path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
			if (Query == INVALID_HANDLE_VALUE) return false;
			HIDD_ATTRIBUTES Attributes = {};
			Attributes.Size = sizeof(Attributes);
			const bool bSony = HidD_GetAttributes(Query, &Attributes) && Attributes.VendorID == SonyVendor;
			CloseHandle(Query);
			EKind Kind;
			if (!bSony || !KindOf(Attributes.ProductID, Kind)) return false;

			// Another tool (DS4Windows with HidHide, Steam) may hold it exclusively; then it simply fails.
			HANDLE File = CreateFileW(*Path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
				OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
			if (File == INVALID_HANDLE_VALUE) return false;

			PHIDP_PREPARSED_DATA Preparsed = nullptr;
			HIDP_CAPS Caps = {};
			if (HidD_GetPreparsedData(File, &Preparsed))
			{
				HidP_GetCaps(Preparsed, &Caps);
				HidD_FreePreparsedData(Preparsed);
			}
			// only the gamepad collection (generic desktop / game pad)
			if (Caps.UsagePage != 0x01 || Caps.Usage != 0x05 || Caps.InputReportByteLength == 0)
			{
				CloseHandle(File);
				return false;
			}
			Out.Path = Path;
			Out.File = File;
			Out.Kind = Kind;
			Out.ReportSize = Caps.InputReportByteLength;
			return true;
		}

		std::atomic<bool> bStop { false };
		FCriticalSection Lock;
		TSet<FString> Known;
		TArray<FFoundPad> Found;
	};

	class FDevice final : public IInputDevice
	{
	public:
		explicit FDevice(const TSharedRef<FGenericApplicationMessageHandler>& InHandler) : Handler(InHandler)
		{
			ScannerThread = FRunnableThread::Create(&Scanner, TEXT("PlayStationPadScanner"), 0, TPri_BelowNormal);
		}

		virtual ~FDevice() override
		{
			if (ScannerThread)
			{
				ScannerThread->Kill(true);
				delete ScannerThread;
			}
			for (int32 i = 0; i < MaxPads; ++i) Close(i);
		}

		virtual void Tick(float DeltaTime) override
		{
			FFoundPad Pad;
			while (Scanner.Take(Pad))
			{
				const int32 Index = FreeSlot();
				if (Index == INDEX_NONE)
				{
					CloseHandle(Pad.File);
					Scanner.Forget(Pad.Path);
					continue;
				}
				FSlot& Slot = Slots[Index];
				Slot.Path = Pad.Path;
				Slot.File = Pad.File;
				Slot.Reader = MakeUnique<FReader>(Pad.File, Pad.Kind, Pad.ReportSize);
				Slot.Thread = FRunnableThread::Create(Slot.Reader.Get(), *FString::Printf(TEXT("PlayStationPad%d"), Index), 0, TPri_AboveNormal);
				Slot.Previous = FState();
				// pad n is controller n, the same ids the Xbox pads would use
				IPlatformInputDeviceMapper::Get().RemapControllerIdToPlatformUserAndDevice(Index, Slot.User, Slot.Device);
				UE_LOG(LogPlayStationPads, Log, TEXT("%s connected as controller %d"),
					Pad.Kind == EKind::DualSense ? TEXT("DualSense") : TEXT("DualShock 4"), Index);
			}
		}

		virtual void SendControllerEvents() override
		{
			const double Now = FPlatformTime::Seconds();
			for (int32 Index = 0; Index < MaxPads; ++Index)
			{
				FSlot& Slot = Slots[Index];
				if (!Slot.Reader) continue;
				if (Slot.Reader->bDead)
				{
					UE_LOG(LogPlayStationPads, Log, TEXT("Controller %d disconnected"), Index);
					Send(Slot, FState(), Now); // let go of everything that was held
					Close(Index);
					continue;
				}
				Send(Slot, Slot.Reader->Get(), Now);
			}
		}

		virtual void SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InHandler) override { Handler = InHandler; }
		virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override { return false; }
		// rumble and light bar come later, they need the output reports
		virtual void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override {}
		virtual void SetChannelValues(int32 ControllerId, const FForceFeedbackValues& Values) override {}

		virtual bool IsGamepadAttached() const override
		{
			for (const FSlot& Slot : Slots)
				if (Slot.Reader) return true;
			return false;
		}

	private:
		struct FSlot
		{
			FString Path;
			HANDLE File = INVALID_HANDLE_VALUE;
			TUniquePtr<FReader> Reader;
			FRunnableThread* Thread = nullptr;
			FState Previous;
			double RepeatAt[ButtonCount] = {};
			FPlatformUserId User;
			FInputDeviceId Device;
		};

		int32 FreeSlot() const
		{
			for (int32 i = 0; i < MaxPads; ++i)
				if (!Slots[i].Reader) return i;
			return INDEX_NONE;
		}

		void Close(int32 Index)
		{
			FSlot& Slot = Slots[Index];
			if (Slot.Thread)
			{
				Slot.Thread->Kill(true);
				delete Slot.Thread;
				Slot.Thread = nullptr;
			}
			Slot.Reader.Reset();
			if (Slot.File != INVALID_HANDLE_VALUE) CloseHandle(Slot.File);
			Slot.File = INVALID_HANDLE_VALUE;
			if (!Slot.Path.IsEmpty()) Scanner.Forget(Slot.Path);
			Slot.Path.Reset();
		}

		// Turns state changes into the same press / release / repeat / analog events XInput sends.
		void Send(FSlot& Slot, const FState& Now, double Time)
		{
			for (int32 Button = 0; Button < ButtonCount; ++Button)
			{
				const FName Key = ButtonKey(Button);
				if (Key.IsNone()) continue;
				const bool bDown = Now.Has(Button);
				const bool bWasDown = Slot.Previous.Has(Button);
				if (bDown && !bWasDown)
				{
					Handler->OnControllerButtonPressed(Key, Slot.User, Slot.Device, false);
					Slot.RepeatAt[Button] = Time + FirstRepeat;
				}
				else if (!bDown && bWasDown)
				{
					Handler->OnControllerButtonReleased(Key, Slot.User, Slot.Device, false);
				}
				else if (bDown && Time >= Slot.RepeatAt[Button])
				{
					Handler->OnControllerButtonPressed(Key, Slot.User, Slot.Device, true);
					Slot.RepeatAt[Button] = Time + NextRepeat;
				}
			}
			for (int32 Axis = 0; Axis < AxisCount; ++Axis)
				if (Now.Axis[Axis] != Slot.Previous.Axis[Axis] || Now.Axis[Axis] != 0.f)
					Handler->OnControllerAnalog(AxisKey(Axis), Slot.User, Slot.Device, Now.Axis[Axis]);
			Slot.Previous = Now;
		}

		TSharedRef<FGenericApplicationMessageHandler> Handler;
		FScanner Scanner;
		FRunnableThread* ScannerThread = nullptr;
		FSlot Slots[MaxPads];
	};
}

class FPlayStationPadsModule : public IInputDeviceModule
{
public:
	virtual TSharedPtr<IInputDevice> CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override
	{
		return MakeShared<PlayStationPads::FDevice>(InMessageHandler);
	}
};

IMPLEMENT_MODULE(FPlayStationPadsModule, PlayStationPads)
