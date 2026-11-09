#pragma once

#include "CoreMinimal.h"
#include "fightcore/Input.h"

class APlayerController;
struct FFightBindings;

// Liest Controller + Tastatur und baut daraus fc::FrameInput für den festen 60-Hz-Tick.
// Kurze Taps zwischen zwei Sim-Ticks gehen nicht verloren (Latch), wichtig bei schwankender FPS (Handhelds).
class FIGHTGAME_API FFightInputSampler
{
public:
	// Jeden Render-Frame aufrufen.
	void Sample(const APlayerController* Own, const APlayerController* KeyboardOwner, const FFightBindings& Pad,
		const FFightBindings& Keys);

	// Einmal pro Sim-Tick. ScreenForwardSign: +1 wenn "Stick rechts" für diesen Kämpfer "vorwärts" ist, sonst -1.
	fc::FrameInput Build(float ScreenForwardSign, float Deadzone);

private:
	uint16 Held = 0;
	uint16 Latched = 0;
	FVector2D Stick = FVector2D::ZeroVector;
	FVector2D Digital = FVector2D::ZeroVector;
};
