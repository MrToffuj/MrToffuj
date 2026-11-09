#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FightTypes.h"
#include "InputCoreTypes.h"

#include "FightGameSettings.generated.h"

// Key bindings for one device. Several keys per action are fine.
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
	UPROPERTY(EditAnywhere, Category = "Buttons") TArray<FKey> Target;
	// Digital directions, the left stick is read on top of these
	UPROPERTY(EditAnywhere, Category = "Directions") TArray<FKey> Up;
	UPROPERTY(EditAnywhere, Category = "Directions") TArray<FKey> Down;
	UPROPERTY(EditAnywhere, Category = "Directions") TArray<FKey> Left;
	UPROPERTY(EditAnywhere, Category = "Directions") TArray<FKey> Right;
};

UENUM(BlueprintType)
enum class EFightBodyType : uint8 { Male, Female };

// One clothing item and which cut to wear. Every cut fits every body.
USTRUCT(BlueprintType)
struct FFightOutfitPick
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Outfit") FName Item;
	UPROPERTY(EditAnywhere, Category = "Outfit") bool bFeminineCut = false;
};

USTRUCT(BlueprintType)
struct FFightLoadout
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Loadout") EFightBody Body = EFightBody::Brawler;
	UPROPERTY(EditAnywhere, Category = "Loadout") EFightTechnique Technique = EFightTechnique::CalamityFlames;
	UPROPERTY(EditAnywhere, Category = "Loadout") EFightAssist Assist = EFightAssist::BladeWolf;
	UPROPERTY(EditAnywhere, Category = "Loadout", meta = (ClampMin = "0", ClampMax = "19")) int32 Team = 0;
	// Set to a technique id from a mod to use it instead of Technique
	UPROPERTY(EditAnywhere, Category = "Loadout") FName ModTechnique;
	// Ids of cursed objects, only as many count as the mode allows
	UPROPERTY(EditAnywhere, Category = "Loadout") TArray<FName> CursedObjects;
	UPROPERTY(EditAnywhere, Category = "Appearance") EFightBodyType BodyType = EFightBodyType::Male;
	UPROPERTY(EditAnywhere, Category = "Appearance") TArray<FFightOutfitPick> Outfit;
};

USTRUCT(BlueprintType)
struct FFightCustomRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Custom", meta = (ClampMin = "2", ClampMax = "20")) int32 Fighters = 20;
	// 0 = everyone for themselves
	UPROPERTY(EditAnywhere, Category = "Custom", meta = (ClampMin = "0", ClampMax = "20")) int32 Teams = 2;
	UPROPERTY(EditAnywhere, Category = "Custom") EFightObjective Objective = EFightObjective::Deathmatch;
	UPROPERTY(EditAnywhere, Category = "Custom") EFightMap Map = EFightMap::OldCapital;
	UPROPERTY(EditAnywhere, Category = "Custom") bool bFriendlyFire = false;
	// 0 = no limit
	UPROPERTY(EditAnywhere, Category = "Custom", meta = (ClampMin = "0")) int32 ScoreLimit = 50;
	UPROPERTY(EditAnywhere, Category = "Custom", meta = (ClampMin = "0")) int32 TimeLimitMinutes = 10;
	// Ignored for elimination
	UPROPERTY(EditAnywhere, Category = "Custom", meta = (ClampMin = "1")) int32 RespawnSeconds = 5;
	UPROPERTY(EditAnywhere, Category = "Custom", meta = (ClampMin = "0", ClampMax = "5")) int32 CursedObjectSlots = 3;
	// Map id from a mod, overrides Map when set
	UPROPERTY(EditAnywhere, Category = "Custom") FName ModMap;
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Fight Game"))
class FIGHTGAME_API UFightGameSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UFightGameSettings();

	// Face buttons are bound by position, so the layout is the same on every pad
	UPROPERTY(Config, EditAnywhere, Category = "Input") FFightBindings Gamepad;
	UPROPERTY(Config, EditAnywhere, Category = "Input") FFightBindings KeyboardP1;
	UPROPERTY(Config, EditAnywhere, Category = "Input") FFightBindings KeyboardP2;
	UPROPERTY(Config, EditAnywhere, Category = "Input", meta = (ClampMin = "0.05", ClampMax = "0.9"))
	float StickDeadzone = 0.35f;

	UPROPERTY(Config, EditAnywhere, Category = "Match") EFightMode Mode = EFightMode::Duel;
	UPROPERTY(Config, EditAnywhere, Category = "Match") FFightCustomRules Custom;
	// Local splitscreen players. Open slots are filled with bots.
	UPROPERTY(Config, EditAnywhere, Category = "Match", meta = (ClampMin = "1", ClampMax = "4")) int32 LocalPlayers = 2;
	UPROPERTY(Config, EditAnywhere, Category = "Match") TArray<FFightLoadout> Loadouts;
	// 0 easy, 1 normal, 2 hard
	UPROPERTY(Config, EditAnywhere, Category = "Match", meta = (ClampMin = "0", ClampMax = "2")) int32 BotLevel = 1;
	UPROPERTY(Config, EditAnywhere, Category = "Match") int32 RestartDelayFrames = 180;

	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "200", ClampMax = "1200"))
	float CameraDistance = 430.f;
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "50", ClampMax = "500"))
	float CameraHeight = 220.f;
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "-200", ClampMax = "200"))
	float CameraShoulderOffset = 60.f;
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "40", ClampMax = "90"))
	float CameraFOV = 60.f;
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "1", ClampMax = "30"))
	float CameraFollowSpeed = 10.f;
	// degrees per second at full right stick, without lock-on
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "30", ClampMax = "720"))
	float CameraTurnSpeed = 200.f;

	// Folder (relative to the project) that is scanned for */mod.json at startup
	UPROPERTY(Config, EditAnywhere, Category = "Mods") FString ModsFolder = TEXT("Mods");

	UPROPERTY(Config, EditAnywhere, Category = "Debug") bool bDrawHitboxes = true;
	UPROPERTY(Config, EditAnywhere, Category = "Debug") bool bShowFrameInfo = true;
};
