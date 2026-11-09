#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FightTypes.h"
#include "InputCoreTypes.h"

#include "FightGameSettings.generated.h"

// Tastenbelegung für einen Gerätetyp. Mehrere Tasten pro Aktion möglich (Rebinding über Projekt-Einstellungen).
USTRUCT(BlueprintType)
struct FFightBindings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Buttons") TArray<FKey> Light;
	UPROPERTY(EditAnywhere, Category = "Buttons") TArray<FKey> Heavy;
	UPROPERTY(EditAnywhere, Category = "Buttons") TArray<FKey> Grab;
	UPROPERTY(EditAnywhere, Category = "Buttons") TArray<FKey> Guard;
	UPROPERTY(EditAnywhere, Category = "Buttons") TArray<FKey> Jump;
	UPROPERTY(EditAnywhere, Category = "Buttons") TArray<FKey> Dash;
	UPROPERTY(EditAnywhere, Category = "Buttons") TArray<FKey> Mod;
	UPROPERTY(EditAnywhere, Category = "Buttons") TArray<FKey> Ult;
	UPROPERTY(EditAnywhere, Category = "Buttons") TArray<FKey> Assist;
	// Digitale Richtungen (D-Pad / Tasten). Analog-Stick wird zusätzlich gelesen.
	UPROPERTY(EditAnywhere, Category = "Directions") TArray<FKey> Up;
	UPROPERTY(EditAnywhere, Category = "Directions") TArray<FKey> Down;
	UPROPERTY(EditAnywhere, Category = "Directions") TArray<FKey> Left;
	UPROPERTY(EditAnywhere, Category = "Directions") TArray<FKey> Right;
};

USTRUCT(BlueprintType)
struct FFightLoadout
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Loadout") EFightBody Body = EFightBody::Brawler;
	UPROPERTY(EditAnywhere, Category = "Loadout") EFightTechnique Technique = EFightTechnique::Glutkern;
	// Nur für Summoner
	UPROPERTY(EditAnywhere, Category = "Loadout") EFightAssist Assist = EFightAssist::BladeWolf;
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Fight Game"))
class FIGHTGAME_API UFightGameSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UFightGameSettings();

	// Controller-Belegung (gilt für jedes Gamepad, positionsbasiert -> gleich auf Xbox/PS/Switch/Deck)
	UPROPERTY(Config, EditAnywhere, Category = "Input") FFightBindings Gamepad;
	UPROPERTY(Config, EditAnywhere, Category = "Input") FFightBindings KeyboardP1;
	UPROPERTY(Config, EditAnywhere, Category = "Input") FFightBindings KeyboardP2;
	UPROPERTY(Config, EditAnywhere, Category = "Input", meta = (ClampMin = "0.05", ClampMax = "0.9"))
	float StickDeadzone = 0.35f;

	UPROPERTY(Config, EditAnywhere, Category = "Match") FFightLoadout Player1;
	UPROPERTY(Config, EditAnywhere, Category = "Match") FFightLoadout Player2;
	// Frames nach Rundenende bis zum automatischen Neustart
	UPROPERTY(Config, EditAnywhere, Category = "Match") int32 RestartDelayFrames = 180;

	// Verfolger-Kamera hinter dem eigenen Kämpfer, Lock-on auf den Gegner
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "200", ClampMax = "1200"))
	float CameraDistance = 430.f;
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "50", ClampMax = "500"))
	float CameraHeight = 220.f;
	// Über-die-Schulter-Versatz nach rechts
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "-200", ClampMax = "200"))
	float CameraShoulderOffset = 60.f;
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "40", ClampMax = "90"))
	float CameraFOV = 60.f;
	// Wie schnell die Kamera der Bewegung folgt (höher = direkter)
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "1", ClampMax = "30"))
	float CameraFollowSpeed = 10.f;

	UPROPERTY(Config, EditAnywhere, Category = "Debug") bool bDrawHitboxes = true;
	UPROPERTY(Config, EditAnywhere, Category = "Debug") bool bShowFrameInfo = true;
};
