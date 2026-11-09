#pragma once

#include "CoreMinimal.h"
#include "fightcore/Input.h"

class APlayerController;
struct FFightBindings;

// Turns pad and keyboard state into fc::FrameInput for the fixed 60 Hz tick. Presses are latched between
// ticks so quick taps survive low or uneven frame rates (handhelds).
class FIGHTGAME_API FFightInputSampler
{
public:
	// Jeden Render-Frame aufrufen.
	void Sample(const APlayerController* Own, const APlayerController* KeyboardOwner, const FFightBindings& Pad,
		const FFightBindings& Keys);

	// Once per sim tick. The camera sits behind the fighter, so the stick maps straight onto the
	// simulation's directions.
	fc::FrameInput Build(float Deadzone);

private:
	uint16 Held = 0;
	uint16 Latched = 0;
	FVector2D Stick = FVector2D::ZeroVector;
	FVector2D Digital = FVector2D::ZeroVector;
};
