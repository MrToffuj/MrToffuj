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
	FVector Forward = FVector::ForwardVector;
	FVector Location = FVector::ZeroVector;
	bool bInitialized = false;
};

// Brücke zwischen FightCore und Unreal:
//  - tickt die deterministische Simulation mit festen 60 Hz (unabhängig von der Render-FPS)
//  - liest Controller/Tastatur der lokalen Spieler (bis zu 4, Splitscreen); freie Slots sind Bots
//  - interpoliert die Optik zwischen zwei Sim-Frames
//  - leitet Simulations-Events an Blueprints/VFX/Zerstörung weiter (OnImpact)
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

	// Feuert für jedes Simulations-Event (Treffer, Wandbruch, Realm, K.O., Begleiter ...).
	// Hier hängen später Niagara-Effekte, Chaos-Zerstörung, Sound und Impact-Frames dran.
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
	// Kämpfer-Index dieses lokalen Controllers (lokale Spieler belegen die ersten Slots), -1 = keiner
	int32 GetPlayerIndex(const APlayerController* PC) const;
	// "P1".."P4" für Menschen, "BOT 5" usw. für Bots
	FString GetFighterLabel(int32 Fighter) const;
	FLinearColor GetFighterColor(int32 Fighter) const;

private:
	void BuildSession();
	void SpawnVisuals();
	void SimulateFrame();
	void HandleEvent(const fc::ImpactEvent& Event);
	void Present(float Alpha, float DeltaSeconds);
	void UpdateCameras(float DeltaSeconds);
	void DrawHitboxes() const;

	TUniquePtr<fc::Session> Session;
	fc::MatchState Previous;
	bool bHasPrevious = false;
	fc::EventSink Events;
	FFightInputSampler Samplers[MaxLocalPlayers];
	int32 LocalPlayers = 1;
	double Accumulator = 0.0;
	int32 RoundOverFrames = 0;

	// Präsentation
	float ShakeTime = 0.f;
	float ShakeStrength = 0.f;
	float ImpactFlash = 0.f;
	FFightChaseCamera CameraState[MaxLocalPlayers];

	UPROPERTY() TArray<TObjectPtr<AFightShapeVisual>> FighterVisuals;
	UPROPERTY() TArray<TObjectPtr<AFightShapeVisual>> CompanionVisuals;
	UPROPERTY() TArray<TObjectPtr<AFightShapeVisual>> ProjectileVisuals;
	UPROPERTY() TArray<TObjectPtr<AFightShapeVisual>> WallVisuals;
	UPROPERTY() TObjectPtr<AFightShapeVisual> FloorVisual;
	// Eine Kamera pro lokalem Spieler
	UPROPERTY() TArray<TObjectPtr<ACameraActor>> Cameras;

	UPROPERTY(VisibleAnywhere, Category = "Fight") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Fight") TObjectPtr<UDirectionalLightComponent> KeyLight;
	UPROPERTY(VisibleAnywhere, Category = "Fight") TObjectPtr<UDirectionalLightComponent> FillLight;
};
