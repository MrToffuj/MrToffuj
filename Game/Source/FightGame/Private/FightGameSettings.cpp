#include "FightGameSettings.h"

UFightGameSettings::UFightGameSettings()
{
	CategoryName = TEXT("Game");

	// Controller: Face-Buttons nach POSITION (unten/links/oben/rechts), dadurch identisch auf allen Pads.
	Gamepad.Light  = { EKeys::Gamepad_FaceButton_Left };
	Gamepad.Heavy  = { EKeys::Gamepad_FaceButton_Top };
	Gamepad.Grab   = { EKeys::Gamepad_FaceButton_Right };
	Gamepad.Jump   = { EKeys::Gamepad_FaceButton_Bottom };
	Gamepad.Guard  = { EKeys::Gamepad_RightShoulder };
	Gamepad.Mod    = { EKeys::Gamepad_LeftShoulder };
	Gamepad.Dash   = { EKeys::Gamepad_RightTrigger };
	Gamepad.Ult    = { EKeys::Gamepad_LeftTrigger };
	Gamepad.Assist = { EKeys::Gamepad_RightThumbstick };
	Gamepad.Up     = { EKeys::Gamepad_DPad_Up };
	Gamepad.Down   = { EKeys::Gamepad_DPad_Down };
	Gamepad.Left   = { EKeys::Gamepad_DPad_Left };
	Gamepad.Right  = { EKeys::Gamepad_DPad_Right };

	KeyboardP1.Light  = { EKeys::J };
	KeyboardP1.Heavy  = { EKeys::K };
	KeyboardP1.Grab   = { EKeys::L };
	KeyboardP1.Jump   = { EKeys::SpaceBar };
	KeyboardP1.Guard  = { EKeys::LeftShift };
	KeyboardP1.Mod    = { EKeys::Q };
	KeyboardP1.Dash   = { EKeys::E };
	KeyboardP1.Ult    = { EKeys::R };
	KeyboardP1.Assist = { EKeys::F };
	KeyboardP1.Up     = { EKeys::W };
	KeyboardP1.Down   = { EKeys::S };
	KeyboardP1.Left   = { EKeys::A };
	KeyboardP1.Right  = { EKeys::D };

	KeyboardP2.Light  = { EKeys::NumPadOne };
	KeyboardP2.Heavy  = { EKeys::NumPadTwo };
	KeyboardP2.Grab   = { EKeys::NumPadThree };
	KeyboardP2.Jump   = { EKeys::NumPadZero };
	KeyboardP2.Guard  = { EKeys::NumPadFour };
	KeyboardP2.Mod    = { EKeys::NumPadFive };
	KeyboardP2.Dash   = { EKeys::NumPadSix };
	KeyboardP2.Ult    = { EKeys::NumPadSeven };
	KeyboardP2.Assist = { EKeys::NumPadEight };
	KeyboardP2.Up     = { EKeys::Up };
	KeyboardP2.Down   = { EKeys::Down };
	KeyboardP2.Left   = { EKeys::Left };
	KeyboardP2.Right  = { EKeys::Right };

	Player1.Body = EFightBody::Summoner;
	Player1.Technique = EFightTechnique::Glutkern;
	Player1.Assist = EFightAssist::BladeWolf;
	Player2.Body = EFightBody::Caster;
	Player2.Technique = EFightTechnique::Nullraum;
}
