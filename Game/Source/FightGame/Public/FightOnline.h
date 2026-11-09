#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "FightEos.h"
#include "FightGameSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "FightOnline.generated.h"

// What the menus picked. The level reloads between menu and match, this survives it.
struct FFightMatchRequest
{
	bool bValid = false;
	EFightMode Mode = EFightMode::Duel;
	FFightCustomRules Custom;
	int32 LocalPlayers = 1;
	EFightNetMode Net = EFightNetMode::Offline;
	FString JoinCode;      // lobby code (EOS) or host address (direct IP)
	bool bForParty = false; // party leader: tell the party where the match is
};

// Lives as long as the game runs: the match request, the EOS login and the party.
//
// A party is a small EOS lobby of friends. The leader picks a mode, hosts the match lobby
// and puts its code into the party lobby; everyone else sees it and joins on their own.
// Strangers can still fill the rest of the match through matchmaking.
UCLASS()
class FIGHTGAME_API UFightOnline : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UFightOnline* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- menu <-> match
	void StartMatch(const FFightMatchRequest& InRequest);
	void ReturnToMenu();
	bool IsInMatch() const { return Request.bValid; }
	const FFightMatchRequest& GetRequest() const { return Request; }
	// For starting a map straight from the editor without going through the menu.
	void SetRequestWithoutTravel(const FFightMatchRequest& InRequest) { Request = InRequest; }

	// --- EOS
	void EnsureLoggedIn();
	FFightEosPlatform& GetPlatform() { return Platform; }
	FString GetOnlineStatus() const { return Platform.GetStatus(); }

	// --- party
	void CreateParty();
	void JoinParty(const FString& Code);
	void LeaveParty();
	bool InParty() const { return Party.IsValid(); }
	bool IsPartyLeader() const;
	int32 GetPartySize() const;
	FString GetPartyCode() const;
	FString GetPartyStatus() const;
	// leader, as soon as its match lobby has a code
	void AnnounceMatch(const FString& MatchCode, EFightMode Mode);
	// member: the leader started a new match since we last looked. Empty if not.
	bool TakePartyMatch(FString& OutCode, EFightMode& OutMode);

private:
	bool Tick(float DeltaSeconds);

	FFightMatchRequest Request;
	FFightEosPlatform Platform;
	TUniquePtr<FFightEosLobby> Party;
	bool bPartySeen = false;
	FString SeenMatch;
	FTSTicker::FDelegateHandle TickHandle;
};
