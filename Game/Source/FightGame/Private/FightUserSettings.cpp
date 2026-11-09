#include "FightUserSettings.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Misc/App.h"

UFightUserSettings* UFightUserSettings::Get()
{
	return Cast<UFightUserSettings>(GEngine ? GEngine->GetGameUserSettings() : nullptr);
}

void UFightUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	const UFightUserSettings* Defaults = GetDefault<UFightUserSettings>();
	MasterVolume = Defaults->MasterVolume;
	MusicVolume = Defaults->MusicVolume;
	EffectsVolume = Defaults->EffectsVolume;
	VoiceVolume = Defaults->VoiceVolume;
	bMuteInBackground = Defaults->bMuteInBackground;
	StickDeadzone = Defaults->StickDeadzone;
	CameraTurnSpeed = Defaults->CameraTurnSpeed;
	bInvertCameraX = Defaults->bInvertCameraX;
	bVibration = Defaults->bVibration;
	InputDelay = Defaults->InputDelay;
	bShowFrameInfo = Defaults->bShowFrameInfo;
	// the name stays, nobody wants to type it again after a reset
}

void UFightUserSettings::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings();
	if (GEngine)
		if (FAudioDeviceHandle Audio = GEngine->GetMainAudioDevice())
			Audio->SetTransientPrimaryVolume(FMath::Clamp(MasterVolume, 0.f, 1.f));
	FApp::SetUnfocusedVolumeMultiplier(bMuteInBackground ? 0.f : 1.f);
	// Music, effects and voice go to their sound classes once the game has sounds;
	// the values are stored already so the menu works.
}
