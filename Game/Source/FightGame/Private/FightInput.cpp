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
		{ &FFightBindings::Assist, fc::BtnAssist },
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
	const FFightBindings& Pad, const FFightBindings& Keys)
{
	Held = 0;
	for (const FButtonMap& B : GButtons)
	{
		// Gamepad-Tasten kommen am eigenen Controller an, die Tastatur immer am Spieler-0-Controller.
		const bool bDown = AnyDown(Own, Pad.*B.Member) || AnyDown(KeyboardOwner, Keys.*B.Member);
		const bool bPressed = AnyJustPressed(Own, Pad.*B.Member) || AnyJustPressed(KeyboardOwner, Keys.*B.Member);
		if (bDown) Held |= B.Bit;
		if (bPressed) Latched |= B.Bit;
	}

	Stick = FVector2D::ZeroVector;
	if (Own)
	{
		Stick.X = Own->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
		Stick.Y = Own->GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
	}
	Digital = FVector2D::ZeroVector;
	if (AnyDown(Own, Pad.Right) || AnyDown(KeyboardOwner, Keys.Right)) Digital.X += 1.f;
	if (AnyDown(Own, Pad.Left) || AnyDown(KeyboardOwner, Keys.Left)) Digital.X -= 1.f;
	if (AnyDown(Own, Pad.Up) || AnyDown(KeyboardOwner, Keys.Up)) Digital.Y += 1.f;
	if (AnyDown(Own, Pad.Down) || AnyDown(KeyboardOwner, Keys.Down)) Digital.Y -= 1.f;
}

fc::FrameInput FFightInputSampler::Build(float ScreenForwardSign, float Deadzone)
{
	fc::FrameInput In;
	In.buttons = static_cast<uint16_t>(Held | Latched);
	Latched = 0;

	// Digital hat Vorrang (D-Pad / Tasten), sonst Analog-Stick mit Deadzone.
	FVector2D V = Digital.IsNearlyZero() ? Stick : Digital;
	const float Mag = V.Size();
	if (Mag < Deadzone)
	{
		In.dir = 5;
		return In;
	}
	V /= Mag;
	// 8-Wege mit 45°-Sektoren: Achse aktiv ab sin(22,5°)
	constexpr float Threshold = 0.383f;
	const int H = V.X * ScreenForwardSign > Threshold ? 1 : (V.X * ScreenForwardSign < -Threshold ? -1 : 0);
	const int Vt = V.Y > Threshold ? 1 : (V.Y < -Threshold ? -1 : 0);
	// Numpad: 7 8 9 / 4 5 6 / 1 2 3, 6 = vorwärts (zum Gegner)
	static const uint8 Numpad[3][3] = { { 1, 2, 3 }, { 4, 5, 6 }, { 7, 8, 9 } };
	In.dir = Numpad[Vt + 1][H + 1];
	return In;
}
