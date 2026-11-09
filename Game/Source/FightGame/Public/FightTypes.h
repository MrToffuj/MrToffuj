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
	Brawl UMETA(DisplayName = "Brawl"),
	Custom UMETA(DisplayName = "Custom")
};

UENUM(BlueprintType)
enum class EFightNetMode : uint8
{
	Offline UMETA(DisplayName = "Offline (local only)"),
	Host UMETA(DisplayName = "Host a P2P lobby"),
	Join UMETA(DisplayName = "Join a P2P lobby"),
	QuickMatch UMETA(DisplayName = "Quick match (find a lobby, host if there is none)")
};

UENUM(BlueprintType)
enum class EFightNetBackend : uint8
{
	Eos UMETA(DisplayName = "EOS (internet, NAT punch-through)"),
	DirectIp UMETA(DisplayName = "Direct IP (LAN / port forwarding)")
};

UENUM(BlueprintType)
enum class EFightObjective : uint8
{
	None UMETA(DisplayName = "Free fight"),
	Elimination,
	Deathmatch,
	CaptureTheFlag UMETA(DisplayName = "Capture the Flag"),
	Oddball,
	Checkpoints
};

UENUM(BlueprintType)
enum class EFightMap : uint8
{
	ShrineWard UMETA(DisplayName = "Shrine Ward (small)"),
	HarborDistrict UMETA(DisplayName = "Harbor District (medium)"),
	OldCapital UMETA(DisplayName = "Old Capital (large)"),
	TempleSteps UMETA(DisplayName = "Temple Steps (medium)"),
	ShatteredDowntown UMETA(DisplayName = "Shattered Downtown (large)"),
	FrozenLake UMETA(DisplayName = "Frozen Lake (medium, open)")
};

UENUM(BlueprintType)
enum class EFightBody : uint8
{
	Heavy, Speedster, Caster, Brawler, Technician, Summoner,
	HeavensBind UMETA(DisplayName = "Heaven's Bind")
};

UENUM(BlueprintType)
enum class EFightTechnique : uint8
{
	None,
	Boundless,
	CalamityFlames UMETA(DisplayName = "Calamity Flames"),
	ShadowPuppets UMETA(DisplayName = "Shadow Puppets"),
	GraniteCannon UMETA(DisplayName = "Granite Cannon"),
	SkyWeave UMETA(DisplayName = "Sky Weave"),
	ThunderVessel UMETA(DisplayName = "Thunder Vessel"),
	StarMass UMETA(DisplayName = "Star Mass"),
	BlossomDraw UMETA(DisplayName = "Blossom Draw"),
	SpiritHoard UMETA(DisplayName = "Spirit Hoard"),
	Jackpot,
	StrawEffigy UMETA(DisplayName = "Straw Effigy"),
	PuppetPilot UMETA(DisplayName = "Puppet Pilot"),
	LightMalice UMETA(DisplayName = "Light & Malice")
};

UENUM(BlueprintType)
enum class EFightAssist : uint8
{
	BladeWolf, SparkOwl, VineToad, StoneWarden
};

// Mirrors fc::ImpactKind, keep the order in sync.
UENUM(BlueprintType)
enum class EFightImpactKind : uint8
{
	Hit, Block, Parry, Nullified, BlackSpark, GuaranteedHit,
	GroundSlam, Landing, DashDust, WallSplat, WallBreak,
	ProjectileBurst, TechniqueFx, Burst,
	RealmOpen, RealmClash, RealmEnd,
	Ignite, Burnout,
	CompanionSummon, CompanionCall, CompanionDown, CompanionRevive, CompanionGone,
	KO, Respawn,
	BarrierHit, FlagTaken, FlagDropped, FlagReturned, FlagCaptured, BallTaken, BallDropped, ZoneCaptured
};

USTRUCT(BlueprintType)
struct FFightImpact
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Fight") int32 Frame = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Fight") EFightImpactKind Kind = EFightImpactKind::Hit;
	UPROPERTY(BlueprintReadOnly, Category = "Fight") int32 Source = 0;
	// 0 light .. 3 crushing
	UPROPERTY(BlueprintReadOnly, Category = "Fight") int32 Strength = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Fight") int32 Tag = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Fight") FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Fight") FVector Direction = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Fight") float Force = 0.f;
};

// Simulation units are meters with x right / y up / z forward; Unreal is centimeters with x forward / y right / z up.
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
