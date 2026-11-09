#pragma once

#include "CoreMinimal.h"
#include "FightInput.h"
#include "FightTypes.h"
#include "GameFramework/Actor.h"
#include "fightcore/Session.h"

#include "FightDirector.generated.h"

class ACameraActor;
class AFightShapeVisual;
class UDirectionalLightComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFightImpact, const FFightImpact&, Impact);

struct FFightChaseCamera
{
	float Yaw = 0.f;
	FVector Forward = FVector::ForwardVector;
	FVector Location = FVector::ZeroVector;
	bool bInitialized = false;
};

// Owns the match. Runs the simulation at a fixed 60 Hz, feeds it local input, draws the
// result and forwards simulation events to whoever wants them (VFX, destruction, audio).
// Unreal never writes into the simulation state.
UCLASS()
class FIGHTGAME_API AFightDirector : public AActor
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxLocalPlayers = 4;
	static constexpr double FixedStep = 1.0 / 60.0;

	AFightDirector();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(BlueprintAssignable, Category = "Fight")
	FOnFightImpact OnImpact;

	UFUNCTION(BlueprintCallable, Category = "Fight")
	void RestartMatch();

	bool HasSession() const { return Session.IsValid(); }
	const fc::MatchState& GetState() const { return Session->state(); }
	const fc::MatchSetup& GetSetup() const { return Session->setup(); }
	int32 GetRollbackCount() const { return Session ? Session->rollbackCount() : 0; }
	float GetImpactFlash() const { return ImpactFlash; }
	int32 GetLocalPlayerCount() const { return LocalPlayers; }

	// Local players own the first fighter slots. Returns -1 for anything else.
	int32 GetPlayerIndex(const APlayerController* PC) const;
	FString GetFighterLabel(int32 Fighter) const;
	// Loads every mod.json under the mods folder once. Safe to call repeatedly.
	static void LoadMods();
	FLinearColor GetFighterColor(int32 Fighter) const;
	FLinearColor GetTeamColor(int32 Team) const;

private:
	void BuildSession();
	void SpawnArena();
	void SpawnFighterVisuals();
	void SimulateFrame();
	void HandleEvent(const fc::ImpactEvent& Event);
	void Present(float Alpha, float DeltaSeconds);
	void PresentObjective();
	void UpdateCameras(float DeltaSeconds);
	void DrawHitboxes() const;
	void RecordMatchResult();
	void ApplyAppearances();

	AFightShapeVisual* SpawnShape(int32 Shape, const FLinearColor& Color, const FVector& SizeCm, const FVector& Offset);

	TUniquePtr<fc::Session> Session;
	fc::MatchState Previous;
	bool bHasPrevious = false;
	fc::EventSink Events;
	FFightInputSampler Samplers[MaxLocalPlayers];
	int32 LocalPlayers = 1;
	double Accumulator = 0.0;
	int32 RoundOverFrames = 0;
	bool bResultRecorded = false;
	int32 BlackSparks[MaxLocalPlayers] = {};
	int32 RealmsOpened[MaxLocalPlayers] = {};

	float ShakeTime = 0.f;
	float ShakeStrength = 0.f;
	float ImpactFlash = 0.f;
	FFightChaseCamera CameraState[MaxLocalPlayers];

	UPROPERTY() TArray<TObjectPtr<AFightShapeVisual>> FighterVisuals;
	UPROPERTY() TArray<TObjectPtr<AFightShapeVisual>> CompanionVisuals;
	UPROPERTY() TArray<TObjectPtr<AFightShapeVisual>> ProjectileVisuals;
	UPROPERTY() TArray<TObjectPtr<AFightShapeVisual>> ObstacleVisuals;
	UPROPERTY() TArray<TObjectPtr<AFightShapeVisual>> BarrierVisuals;
	UPROPERTY() TArray<TObjectPtr<AFightShapeVisual>> FlagVisuals;
	UPROPERTY() TArray<TObjectPtr<AFightShapeVisual>> ZoneVisuals;
	UPROPERTY() TObjectPtr<AFightShapeVisual> BallVisual;
	UPROPERTY() TObjectPtr<AFightShapeVisual> FloorVisual;
	UPROPERTY() TArray<TObjectPtr<ACameraActor>> Cameras;

	UPROPERTY(VisibleAnywhere, Category = "Fight") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Fight") TObjectPtr<UDirectionalLightComponent> KeyLight;
	UPROPERTY(VisibleAnywhere, Category = "Fight") TObjectPtr<UDirectionalLightComponent> FillLight;
};
