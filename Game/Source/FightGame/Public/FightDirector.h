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

// Brücke zwischen FightCore und Unreal:
//  - tickt die deterministische Simulation mit festen 60 Hz (unabhängig von der Render-FPS)
//  - liest Controller/Tastatur für beide Spieler
//  - interpoliert die Optik zwischen zwei Sim-Frames
//  - leitet Simulations-Events an Blueprints/VFX/Zerstörung weiter (OnImpact)
UCLASS()
class FIGHTGAME_API AFightDirector : public AActor
{
	GENERATED_BODY()

public:
	AFightDirector();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// Feuert für jedes Simulations-Event (Treffer, Wandbruch, Realm, Begleiter ...).
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

	static constexpr double FixedStep = 1.0 / 60.0;

private:
	void BuildSession();
	void SpawnVisuals();
	void SimulateFrame();
	void HandleEvent(const fc::ImpactEvent& Event);
	void Present(float Alpha, float DeltaSeconds);
	void UpdateCamera(float DeltaSeconds);
	void DrawHitboxes() const;
	float ScreenForwardSign(int32 Player) const;

	TUniquePtr<fc::Session> Session;
	fc::MatchState Previous;
	bool bHasPrevious = false;
	fc::EventSink Events;
	FFightInputSampler Samplers[2];
	double Accumulator = 0.0;
	int32 RoundOverFrames = 0;

	// Präsentation
	float ShakeTime = 0.f;
	float ShakeStrength = 0.f;
	float ImpactFlash = 0.f;
	FVector CameraForward = FVector::ForwardVector;
	FVector CameraLocation = FVector::ZeroVector;
	bool bCameraInitialized = false;

	UPROPERTY() TArray<TObjectPtr<AFightShapeVisual>> FighterVisuals;
	UPROPERTY() TArray<TObjectPtr<AFightShapeVisual>> CompanionVisuals;
	UPROPERTY() TArray<TObjectPtr<AFightShapeVisual>> ProjectileVisuals;
	UPROPERTY() TArray<TObjectPtr<AFightShapeVisual>> WallVisuals;
	UPROPERTY() TObjectPtr<AFightShapeVisual> FloorVisual;
	UPROPERTY() TObjectPtr<ACameraActor> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Fight") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Fight") TObjectPtr<UDirectionalLightComponent> KeyLight;
	UPROPERTY(VisibleAnywhere, Category = "Fight") TObjectPtr<UDirectionalLightComponent> FillLight;
};
