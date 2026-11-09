#pragma once

#include "CoreMinimal.h"
#include "FightInput.h"
#include "FightEos.h"
#include "FightNet.h"
#include "FightTypes.h"
#include "GameFramework/Actor.h"
#include "fightcore/Net.h"
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
//
// Offline it drives an fc::Session directly. Online it first sits in the lobby (host or
// join, see the Online settings) and then drives an fc::net::PeerSession instead; the
// rest of the director only ever sees "the session" and which fighters are local.
UCLASS()
class FIGHTGAME_API AFightDirector : public AActor
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxLocalPlayers = 4;
	static constexpr double FixedStep = 1.0 / 60.0;

	AFightDirector();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(BlueprintAssignable, Category = "Fight")
	FOnFightImpact OnImpact;

	UFUNCTION(BlueprintCallable, Category = "Fight")
	void RestartMatch();

	bool HasSession() const { return Session.IsValid() || Peer.IsValid(); }
	const fc::MatchState& GetState() const { return Sim().state(); }
	const fc::MatchSetup& GetSetup() const { return Sim().setup(); }
	int32 GetRollbackCount() const { return HasSession() ? Sim().rollbackCount() : 0; }
	bool IsOnline() const { return NetMode != EFightNetMode::Offline; }
	// Lobby progress, lag spikes, desyncs. Empty when there's nothing to say.
	FString GetNetStatus() const;
	float GetImpactFlash() const { return ImpactFlash; }
	int32 GetLocalPlayerCount() const { return LocalPlayers; }

	// Index among the players on this console, -1 for anything else.
	int32 GetPlayerIndex(const APlayerController* PC) const;
	// The fighter slot a local player controls. Offline that's the first slots, online
	// the host decides.
	int32 GetLocalFighter(int32 LocalIndex) const { return LocalFighters[LocalIndex]; }
	int32 GetPlayerFighter(const APlayerController* PC) const;
	int32 FindLocalIndex(int32 Fighter) const;
	FString GetFighterLabel(int32 Fighter) const;
	// Loads every mod.json under the mods folder once. Safe to call repeatedly.
	static void LoadMods();
	FLinearColor GetFighterColor(int32 Fighter) const;
	FLinearColor GetTeamColor(int32 Team) const;

private:
	const fc::Session& Sim() const { return Peer ? Peer->session() : *Session; }
	void BuildSession();
	void StartPresentation();
	void StartOnline();
	void PollLobby(float DeltaSeconds);
	bool BeginOnlineMatch(const fc::net::LobbyConfig& Config, uint8 LocalPeer);
	void SimulateOnlineFrame();
	void Rematch();
	void SpawnArena();
	void SpawnFighterVisuals();
	void SimulateFrame();
	void HandleEvent(const fc::ImpactEvent& Event);
	void Present(float Alpha, float DeltaSeconds);
	void PresentObjective();
	void UpdateCameras(float DeltaSeconds);
	void DrawHitboxes() const;
	void RecordMatchResult(const fc::MatchState& State);
	void ApplyAppearances();

	AFightShapeVisual* SpawnShape(int32 Shape, const FLinearColor& Color, const FVector& SizeCm, const FVector& Offset);

	TUniquePtr<fc::Session> Session;

	// online
	EFightNetMode NetMode = EFightNetMode::Offline;
	// one of these carries the packets, Link points at whichever it is
	TUniquePtr<FFightUdpTransport> Udp;
	TUniquePtr<FFightEos> Eos;
	IFightTransport* Link = nullptr;
	std::vector<fc::net::PlayerRequest> PendingRequests; // until EOS has a lobby
	TUniquePtr<fc::net::LobbyHost> LobbyHost;
	TUniquePtr<fc::net::LobbyClient> LobbyClient;
	TUniquePtr<fc::net::PeerSession> Peer;
	fc::net::LobbyConfig Lobby;
	fc::MatchSetup OnlineSetup;
	uint8 OwnPeer = 0;
	uint8 Round = 0;
	float LobbyTime = 0.f;
	float LobbyPollTimer = 0.f;
	int32 StartResends = 0;
	FString NetError;
	bool bDesyncReported = false;

	int32 LocalFighters[MaxLocalPlayers] = { 0, 1, 2, 3 };
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
