#include "FightInput.h"

#include "FightGameSettings.h"
#include "GameFramework/PlayerController.h"

namespace
{
	struct FButtonMap
	{
		TArray<FKey> FFightBindings::* Member;
		uint16 Bit;
	};

	const FButtonMap GButtons[] = {
		{ &FFightBindings::Light, fc::BtnLight },   { &FFightBindings::Heavy, fc::BtnHeavy },
		{ &FFightBindings::Grab, fc::BtnGrab },     { &FFightBindings::Guard, fc::BtnGuard },
		{ &FFightBindings::Jump, fc::BtnJump },     { &FFightBindings::Dash, fc::BtnDash },
		{ &FFightBindings::Mod, fc::BtnMod },       { &FFightBindings::Ult, fc::BtnUlt },
		{ &FFightBindings::Assist, fc::BtnAssist }, { &FFightBindings::Target, fc::BtnTarget },
	};

	bool AnyDown(const APlayerController* PC, const TArray<FKey>& Keys)
	{
		if (!PC) return false;
		for (const FKey& K : Keys)
			if (PC->IsInputKeyDown(K)) return true;
		return false;
	}

	bool AnyJustPressed(const APlayerController* PC, const TArray<FKey>& Keys)
	{
		if (!PC) return false;
		for (const FKey& K : Keys)
			if (PC->WasInputKeyJustPressed(K)) return true;
		return false;
	}
}

void FFightInputSampler::Sample(const APlayerController* Own, const APlayerController* KeyboardOwner,
	const FFightBindings& Pad, const FFightBindings& Keys, bool bReadMouse)
{
	Held = 0;
	for (const FButtonMap& B : GButtons)
	{
		// pads report to their own controller, the keyboard always reports to the first one
		const bool bDown = AnyDown(Own, Pad.*B.Member) || AnyDown(KeyboardOwner, Keys.*B.Member);
		const bool bPressed = AnyJustPressed(Own, Pad.*B.Member) || AnyJustPressed(KeyboardOwner, Keys.*B.Member);
		if (bDown) Held |= B.Bit;
		if (bPressed) Latched |= B.Bit;
	}

	Stick = FVector2D::ZeroVector;
	LookYaw = 0.f;
	if (Own)
	{
		Stick.X = Own->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
		Stick.Y = Own->GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
		const float RightX = Own->GetInputAnalogKeyState(EKeys::Gamepad_RightX);
		LookYaw = RightX;
		// flick to switch targets: needs to come back to the middle before the next one counts
		if (bFlickArmed && FMath::Abs(RightX) > 0.85f)
		{
			Latched |= fc::BtnTargetNext;
			bFlickArmed = false;
		}
		else if (FMath::Abs(RightX) < 0.3f)
		{
			bFlickArmed = true;
		}
	}
	if (bReadMouse && KeyboardOwner)
	{
		float MouseX = 0.f, MouseY = 0.f;
		KeyboardOwner->GetInputMouseDelta(MouseX, MouseY);
		LookYaw += MouseX * 0.25f;
	}

	Digital = FVector2D::ZeroVector;
	if (AnyDown(Own, Pad.Right) || AnyDown(KeyboardOwner, Keys.Right)) Digital.X += 1.f;
	if (AnyDown(Own, Pad.Left) || AnyDown(KeyboardOwner, Keys.Left)) Digital.X -= 1.f;
	if (AnyDown(Own, Pad.Up) || AnyDown(KeyboardOwner, Keys.Up)) Digital.Y += 1.f;
	if (AnyDown(Own, Pad.Down) || AnyDown(KeyboardOwner, Keys.Down)) Digital.Y -= 1.f;
}

fc::FrameInput FFightInputSampler::Build(float Deadzone, float CameraYawDegrees)
{
	fc::FrameInput In;
	In.buttons = static_cast<uint16_t>(Held | Latched);
	Latched = 0;

	// d-pad and keys win over the analog stick
	FVector2D V = Digital.IsNearlyZero() ? Stick : Digital;
	const float Mag = V.Size();
	if (Mag < Deadzone) return In;
	V /= Mag;

	// stick space -> world: up on the stick is where the camera looks
	const float Yaw = FMath::DegreesToRadians(CameraYawDegrees);
	const FVector Forward(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.f);
	const FVector Right(-Forward.Y, Forward.X, 0.f);
	const FVector World = Forward * V.Y + Right * V.X;

	// Unreal x forward / y right -> simulation x right / z forward
	In.moveX = static_cast<int8_t>(FMath::Clamp(FMath::RoundToInt(World.Y * 127.f), -127, 127));
	In.moveZ = static_cast<int8_t>(FMath::Clamp(FMath::RoundToInt(World.X * 127.f), -127, 127));
	if (In.moveX == 0 && In.moveZ == 0) In.moveZ = 1; // tiny pushes still count
	return In;
}
