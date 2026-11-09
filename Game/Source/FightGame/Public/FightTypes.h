#pragma once

#include "CoreMinimal.h"
#include "fightcore/Events.h"
#include "fightcore/Fixed.h"

#include "FightTypes.generated.h"

UENUM(BlueprintType)
enum class EFightMode : uint8
{
	Duel UMETA(DisplayName = "1v1"),
	Teams UMETA(DisplayName = "2v2"),
	Brawl UMETA(DisplayName = "Brawl (bis 20)")
};

UENUM(BlueprintType)
enum class EFightBody : uint8
{
	Heavy, Speedster, Caster, Brawler, Technician, Summoner, HeavensBind
};

UENUM(BlueprintType)
enum class EFightTechnique : uint8
{
	None, Boundless, CalamityFlames, ShadowPuppets
};

UENUM(BlueprintType)
enum class EFightAssist : uint8
{
	BladeWolf, SparkOwl, VineToad, StoneWarden
};

// Spiegel von fc::ImpactKind – Reihenfolge MUSS identisch sein (static_assert in FightTypes.cpp).
UENUM(BlueprintType)
enum class EFightImpactKind : uint8
{
	Hit, Block, Parry, Nullified, BlackSpark, GuaranteedHit,
	GroundSlam, Landing, DashDust, WallSplat, WallBreak,
	ProjectileBurst, TechniqueFx, Burst,
	RealmOpen, RealmClash, RealmEnd,
	Ignite, Burnout,
	CompanionSummon, CompanionCall, CompanionDown, CompanionRevive, CompanionGone,
	KO, Respawn
};

// Ein Event aus der Simulation für die Präsentation (Zerstörung, VFX, Sound, Kamera).
USTRUCT(BlueprintType)
struct FFightImpact
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Fight") int32 Frame = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Fight") EFightImpactKind Kind = EFightImpactKind::Hit;
	UPROPERTY(BlueprintReadOnly, Category = "Fight") int32 Source = 0;
	// 0 Light, 1 Medium, 2 Heavy, 3 Crushing
	UPROPERTY(BlueprintReadOnly, Category = "Fight") int32 Strength = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Fight") int32 Tag = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Fight") FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Fight") FVector Direction = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Fight") float Force = 0.f;
};

// Umrechnung Simulation -> Unreal.
// Simulation: Meter, X rechts / Y oben / Z vorne.  Unreal: Zentimeter, X vorne / Y rechts / Z oben.
namespace FightUnits
{
	constexpr float CmPerUnit = 100.f;

	inline FVector ToWorld(const fc::Vec3& V)
	{
		return FVector(V.z.toFloat(), V.x.toFloat(), V.y.toFloat()) * CmPerUnit;
	}
	inline FVector ToWorldDir(const fc::Vec3& V)
	{
		return FVector(V.z.toFloat(), V.x.toFloat(), V.y.toFloat());
	}
	inline float ToCm(fc::Fx V) { return V.toFloat() * CmPerUnit; }

	FFightImpact ToImpact(const fc::ImpactEvent& E);
}
