#pragma once

#include "CoreMinimal.h"
#include "fightcore/Input.h"

class APlayerController;
struct FFightBindings;

// Turns pad and keyboard state into fc::FrameInput for the fixed 60 Hz tick. Presses are latched between
// ticks so quick taps survive low or uneven frame rates (handhelds).
//
// The left stick is turned into a world space direction using the camera yaw, so without lock-on you
// simply run where you push. The right stick turns the camera; a hard flick sideways asks for the next
// lock-on target.
class FIGHTGAME_API FFightInputSampler
{
public:
	// every render frame
	void Sample(const APlayerController* Own, const APlayerController* KeyboardOwner, const FFightBindings& Pad,
		const FFightBindings& Keys, bool bReadMouse);

	// once per sim tick
	fc::FrameInput Build(float Deadzone, float CameraYawDegrees);

	// camera turn input, -1..1 (stick) plus mouse delta
	float GetLookYaw() const { return LookYaw; }

private:
	uint16 Held = 0;
	uint16 Latched = 0;
	FVector2D Stick = FVector2D::ZeroVector;
	FVector2D Digital = FVector2D::ZeroVector;
	float LookYaw = 0.f;
	bool bFlickArmed = true;
};
