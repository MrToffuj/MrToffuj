#pragma once

#include "CoreMinimal.h"
#include "FightGameSettings.h"
#include "GameFramework/GameUserSettings.h"

#include "FightUserSettings.generated.h"

// Everything a player can change from the settings menu. Video goes through the engine's
// own settings (resolution, window mode, vsync, scalability), the rest lives here and ends
// up in the user's GameUserSettings.ini, not in the project config.
UCLASS(Config = GameUserSettings, ConfigDoNotCheckDefaults)
class FIGHTGAME_API UFightUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	static UFightUserSettings* Get();

	virtual void SetToDefaults() override;
	virtual void ApplyNonResolutionSettings() override;

	// 0..1
	UPROPERTY(Config) float MasterVolume = 1.f;
	UPROPERTY(Config) float MusicVolume = 0.8f;
	UPROPERTY(Config) float EffectsVolume = 1.f;
	UPROPERTY(Config) float VoiceVolume = 1.f;
	UPROPERTY(Config) bool bMuteInBackground = true;

	UPROPERTY(Config) float StickDeadzone = 0.35f;
	UPROPERTY(Config) float CameraTurnSpeed = 200.f;
	UPROPERTY(Config) bool bInvertCameraX = false;
	UPROPERTY(Config) bool bVibration = true;

	UPROPERTY(Config) FString DisplayName = TEXT("Player");
	UPROPERTY(Config) int32 InputDelay = 2;
	UPROPERTY(Config) bool bShowFrameInfo = false;

	// what each local player picked last time, so the character select starts there
	UPROPERTY(Config) TArray<FFightLoadout> LastLoadouts;
};
