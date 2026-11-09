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

// Logs in, finds or creates an EOS lobby and hands out a ready transport.
// The EOS lobby is only matchmaking: who plays with whom. The actual handshake
// (versions, mods, loadouts, slots) is still fc::net's Hello/Start over P2P.
class FIGHTGAME_API FFightEos
{
public:
	enum class EStep : uint8 { Off, LoggingIn, Searching, CreatingLobby, JoiningLobby, Ready, Failed };

	struct FConfig
	{
		FString ProductId, SandboxId, DeploymentId, ClientId, ClientSecret;
		FString DisplayName = TEXT("Player");
		// Only lobbies in the same bucket see each other. The director puts the game
		// version, mode and mod hash in there, so nobody gets matched with a different build.
		FString Bucket;
		FString LobbyCode; // join exactly this lobby instead of searching
		bool bHost = false;
		int32 MaxMembers = 2;
	};

	FFightEos();
	~FFightEos();

	void Begin(const FConfig& InConfig);
	void Tick(float DeltaSeconds);
	// Host, once the match starts: the lobby stops showing up in searches.
	void LockLobby();
	void Shutdown();

	EStep GetStep() const { return Step; }
	bool IsReady() const { return Step == EStep::Ready; }
	FString GetStatus() const;
	FString GetLobbyId() const { return LobbyId; }
	IFightTransport* GetTransport() { return IsReady() ? Transport.Get() : nullptr; }

private:
	static FFightEos* Alive(void* ClientData);
	static void EOS_CALL OnDeviceId(const EOS_Connect_CreateDeviceIdCallbackInfo* Data);
	static void EOS_CALL OnLogin(const EOS_Connect_LoginCallbackInfo* Data);
	static void EOS_CALL OnUserCreated(const EOS_Connect_CreateUserCallbackInfo* Data);
	static void EOS_CALL OnLobbyCreated(const EOS_Lobby_CreateLobbyCallbackInfo* Data);
	static void EOS_CALL OnSearchDone(const EOS_LobbySearch_FindCallbackInfo* Data);
	static void EOS_CALL OnJoined(const EOS_Lobby_JoinLobbyCallbackInfo* Data);
	static void EOS_CALL OnLobbyUpdated(const EOS_Lobby_UpdateLobbyCallbackInfo* Data);
	static void EOS_CALL OnLeft(const EOS_Lobby_LeaveLobbyCallbackInfo* Data);

	void Login();
	void LoggedIn(EOS_ProductUserId User);
	void CreateLobby();
	void Search();
	void Fail(const FString& Why);

	FConfig Config;
	EStep Step = EStep::Off;
	FString Error;
	IEOSPlatformHandlePtr Platform;
	EOS_HPlatform Handle = nullptr;
	EOS_ProductUserId Me = nullptr;
	EOS_HLobbySearch SearchHandle = nullptr;
	EOS_HLobbyDetails PendingJoin = nullptr;
	FString PendingHost;
	FString LobbyId;
	float RetryTimer = 0.f;
	TUniquePtr<FFightEosTransport> Transport;

	// EOS strings have to stay alive while the SDK holds the pointers
	std::string ProductId, SandboxId, DeploymentId, ClientId, ClientSecret, CacheDir, DisplayName, Bucket;
};
