#pragma once

#include "CoreMinimal.h"
#include "FightNet.h"
#include "IEOSSDKManager.h"
#include "eos_connect.h"
#include "eos_lobby.h"
#include "eos_p2p.h"
#include "eos_sdk.h"

// fc::net over EOS P2P. EOS does the NAT punch-through and falls back to its relays when
// two consoles can't reach each other, so this works over the internet without servers
// of our own. Peers are addressed by product user id.
//
// EOS packets max out at 1170 bytes. Match packets are far below that, only the Start
// packet of a big lobby isn't, so anything larger goes out in pieces on a reliable channel.
class FIGHTGAME_API FFightEosTransport final : public IFightTransport
{
public:
	~FFightEosTransport();

	void Begin(EOS_HP2P InP2P, EOS_ProductUserId InSelf);
	void Shutdown();

	virtual void SetAcceptNewPeers(bool bAccept) override { bAcceptNewPeers = bAccept; }
	virtual bool SetPeer(uint8 Id, const FString& Address) override;
	virtual TArray<FString> GetPeerAddresses() const override;
	virtual FString Describe() const override { return Description; }
	void SetDescription(const FString& Text) { Description = Text; }

	virtual void send(uint8_t Peer, const std::vector<uint8_t>& Packet) override;
	virtual bool receive(uint8_t& Peer, std::vector<uint8_t>& Packet) override;

	static FString KeyOf(EOS_ProductUserId Id);

private:
	struct FPeer
	{
		EOS_ProductUserId Id = nullptr;
		FString Key;
		TArray<uint8> Partial; // a big packet still coming in
	};

	static void EOS_CALL OnConnectionRequest(const EOS_P2P_OnIncomingConnectionRequestInfo* Data);
	int32 FindPeer(const FString& Key) const;
	void SendRaw(EOS_ProductUserId To, const uint8* Data, int32 Size, uint8 Channel, EOS_EPacketReliability Reliability);

	EOS_HP2P P2P = nullptr;
	EOS_ProductUserId Self = nullptr;
	EOS_P2P_SocketId Socket = {};
	EOS_NotificationId ConnectionNotify = EOS_INVALID_NOTIFICATIONID;
	TArray<FPeer> Peers; // index = peer id
	TArray<uint8> Buffer;
	FString Description;
	bool bAcceptNewPeers = false;
};

// The EOS platform and the login. One per game run (UFightOnline owns it), so the party
// and the match lobby share it and nobody logs in twice.
//
// Device id login: no account, no password, the id sticks to this install. Consoles log in
// with their platform token instead (PSN, Xbox, Nintendo), which comes from the platform
// SDKs and only builds on dev kits.
class FIGHTGAME_API FFightEosPlatform
{
public:
	struct FConfig
	{
		FString ProductId, SandboxId, DeploymentId, ClientId, ClientSecret;
		FString DisplayName = TEXT("Player");
	};

	FFightEosPlatform();
	~FFightEosPlatform();

	// Safe to call again, it only starts once.
	void Begin(const FConfig& Config);
	void Shutdown();

	bool IsLoggedIn() const { return Me != nullptr; }
	bool HasFailed() const { return !Error.IsEmpty(); }
	FString GetStatus() const;
	EOS_HPlatform GetHandle() const { return Handle; }
	EOS_ProductUserId GetUser() const { return Me; }

private:
	static void EOS_CALL OnDeviceId(const EOS_Connect_CreateDeviceIdCallbackInfo* Data);
	static void EOS_CALL OnLogin(const EOS_Connect_LoginCallbackInfo* Data);
	static void EOS_CALL OnUserCreated(const EOS_Connect_CreateUserCallbackInfo* Data);
	void Fail(const FString& Why);

	IEOSPlatformHandlePtr Platform;
	EOS_HPlatform Handle = nullptr;
	EOS_ProductUserId Me = nullptr;
	bool bStarted = false;
	FString Error;
	// EOS keeps pointers to these
	std::string ProductId, SandboxId, DeploymentId, ClientId, ClientSecret, CacheDir, DisplayName;
};

// One EOS lobby: a match lobby (with a P2P transport for fc::net) or a party (just a group
// of friends that follows its leader into matches).
//
// For matches the EOS lobby is only matchmaking. The real handshake (versions, mods,
// loadouts, slots) is still fc::net's Hello/Start over the transport.
class FIGHTGAME_API FFightEosLobby
{
public:
	enum class EJoin : uint8
	{
		Host, // open a lobby
		Join, // find one (or the one in LobbyCode) and keep looking until it works
		Auto  // look for a while, open our own if nothing turns up
	};
	enum class EStep : uint8 { WaitingForLogin, Searching, CreatingLobby, JoiningLobby, Ready, Failed, Closed };

	struct FConfig
	{
		EJoin Join = EJoin::Auto;
		// Only lobbies in the same bucket see each other. The director puts the game version,
		// mode and mod hash in there, so nobody gets matched with a different build.
		FString Bucket;
		FString LobbyCode;
		int32 MaxMembers = 2;
		bool bTransport = true;          // false for parties
		float AutoSearchSeconds = 15.f;  // EJoin::Auto: how long to look before hosting
	};

	explicit FFightEosLobby(FFightEosPlatform& InPlatform);
	~FFightEosLobby();

	void Begin(const FConfig& InConfig);
	void Tick(float DeltaSeconds);
	void Shutdown();

	// Host only. Lobby attributes are how the party leader tells everyone which match to join.
	void SetAttribute(const FString& Key, const FString& Value);
	// Members see the host's attributes. Refreshed in Tick.
	FString GetAttribute(const FString& Key) const;
	int32 GetMemberCount() const { return MemberCount; }
	// Host, once the match starts: the lobby stops showing up in searches.
	void Lock();

	EStep GetStep() const { return Step; }
	bool IsReady() const { return Step == EStep::Ready; }
	bool IsHost() const { return bHosting; }
	FString GetStatus() const;
	FString GetLobbyId() const { return LobbyId; }
	IFightTransport* GetTransport() { return IsReady() ? Transport.Get() : nullptr; }

private:
	static FFightEosLobby* Alive(void* ClientData);
	static void EOS_CALL OnLobbyCreated(const EOS_Lobby_CreateLobbyCallbackInfo* Data);
	static void EOS_CALL OnSearchDone(const EOS_LobbySearch_FindCallbackInfo* Data);
	static void EOS_CALL OnJoined(const EOS_Lobby_JoinLobbyCallbackInfo* Data);
	static void EOS_CALL OnLobbyUpdated(const EOS_Lobby_UpdateLobbyCallbackInfo* Data);
	static void EOS_CALL OnLeft(const EOS_Lobby_LeaveLobbyCallbackInfo* Data);

	void Start();
	void CreateLobby();
	void Search();
	void BecomeReady(const char* Id, bool bAsHost);
	void Refresh();
	void Fail(const FString& Why);
	EOS_HLobby Lobbies() const;

	FFightEosPlatform& Platform;
	FConfig Config;
	EStep Step = EStep::WaitingForLogin;
	FString Error;
	bool bHosting = false;
	EOS_HLobbySearch SearchHandle = nullptr;
	EOS_HLobbyDetails PendingJoin = nullptr;
	FString PendingHost;
	FString LobbyId;
	float RetryTimer = 0.f;
	float SearchTime = 0.f;
	float RefreshTimer = 0.f;
	int32 MemberCount = 0;
	TMap<FString, FString> Attributes;
	TUniquePtr<FFightEosTransport> Transport;
	std::string Bucket;
};
