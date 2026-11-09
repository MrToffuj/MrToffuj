#include "FightEos.h"

#include "FightGame.h"
#include "Misc/Paths.h"

namespace
{
	constexpr int32 MaxEosPacket = EOS_P2P_MAX_PACKET_SIZE;
	constexpr int32 PieceHeader = 2;                  // index, count
	constexpr int32 PieceSize = MaxEosPacket - PieceHeader;
	constexpr uint8 ChannelMatch = 0;                 // unreliable, the netcode resends itself
	constexpr uint8 ChannelPieces = 1;                // reliable + ordered, for big packets only
	constexpr float SearchRetrySeconds = 3.f;

	std::string Utf8(const FString& Text) { return std::string(TCHAR_TO_UTF8(*Text)); }
	FString ResultText(EOS_EResult Result) { return UTF8_TO_TCHAR(EOS_EResult_ToString(Result)); }

	// callbacks can still arrive after we're gone, they check this first
	TSet<void*> GLive;
}

// ---------------------------------------------------------------- transport

FFightEosTransport::~FFightEosTransport()
{
	Shutdown();
}

void FFightEosTransport::Begin(EOS_HP2P InP2P, EOS_ProductUserId InSelf)
{
	P2P = InP2P;
	Self = InSelf;
	Socket = {};
	Socket.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
	FCStringAnsi::Strncpy(Socket.SocketName, "FightP2P", sizeof(Socket.SocketName));
	Buffer.SetNumUninitialized(MaxEosPacket);
	Peers.SetNum(1);
	GLive.Add(this);

	EOS_P2P_AddNotifyPeerConnectionRequestOptions Options = {};
	Options.ApiVersion = EOS_P2P_ADDNOTIFYPEERCONNECTIONREQUEST_API_LATEST;
	Options.LocalUserId = Self;
	Options.SocketId = &Socket;
	ConnectionNotify = EOS_P2P_AddNotifyPeerConnectionRequest(P2P, &Options, this, &FFightEosTransport::OnConnectionRequest);
}

void FFightEosTransport::Shutdown()
{
	GLive.Remove(this);
	if (!P2P) return;
	if (ConnectionNotify != EOS_INVALID_NOTIFICATIONID) EOS_P2P_RemoveNotifyPeerConnectionRequest(P2P, ConnectionNotify);
	ConnectionNotify = EOS_INVALID_NOTIFICATIONID;

	EOS_P2P_CloseConnectionsOptions Close = {};
	Close.ApiVersion = EOS_P2P_CLOSECONNECTIONS_API_LATEST;
	Close.LocalUserId = Self;
	Close.SocketId = &Socket;
	EOS_P2P_CloseConnections(P2P, &Close);
	P2P = nullptr;
}

void EOS_CALL FFightEosTransport::OnConnectionRequest(const EOS_P2P_OnIncomingConnectionRequestInfo* Data)
{
	FFightEosTransport* T = static_cast<FFightEosTransport*>(Data->ClientData);
	if (!GLive.Contains(T) || !T->P2P) return;
	// Known peers always get in. Strangers only while the host is collecting players.
	const bool bKnown = T->FindPeer(KeyOf(Data->RemoteUserId)) != INDEX_NONE;
	if (!bKnown && !T->bAcceptNewPeers) return;

	EOS_P2P_AcceptConnectionOptions Accept = {};
	Accept.ApiVersion = EOS_P2P_ACCEPTCONNECTION_API_LATEST;
	Accept.LocalUserId = T->Self;
	Accept.RemoteUserId = Data->RemoteUserId;
	Accept.SocketId = Data->SocketId;
	EOS_P2P_AcceptConnection(T->P2P, &Accept);
}

FString FFightEosTransport::KeyOf(EOS_ProductUserId Id)
{
	char Text[EOS_PRODUCTUSERID_MAX_LENGTH + 1] = {};
	int32_t Length = sizeof(Text);
	if (!Id || EOS_ProductUserId_ToString(Id, Text, &Length) != EOS_EResult::EOS_Success) return FString();
	return UTF8_TO_TCHAR(Text);
}

int32 FFightEosTransport::FindPeer(const FString& Key) const
{
	if (Key.IsEmpty()) return INDEX_NONE;
	for (int32 i = 0; i < Peers.Num(); ++i)
		if (Peers[i].Key == Key) return i;
	return INDEX_NONE;
}

bool FFightEosTransport::SetPeer(uint8 Id, const FString& Address)
{
	const EOS_ProductUserId User = EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*Address));
	if (!EOS_ProductUserId_IsValid(User)) return false;
	if (Peers.Num() <= Id) Peers.SetNum(Id + 1);
	Peers[Id].Id = User;
	Peers[Id].Key = KeyOf(User);
	Peers[Id].Partial.Reset();
	return true;
}

TArray<FString> FFightEosTransport::GetPeerAddresses() const
{
	TArray<FString> Out;
	for (const FPeer& P : Peers) Out.Add(P.Key);
	return Out;
}

void FFightEosTransport::SendRaw(EOS_ProductUserId To, const uint8* Data, int32 Size, uint8 Channel, EOS_EPacketReliability Reliability)
{
	EOS_P2P_SendPacketOptions Options = {};
	Options.ApiVersion = EOS_P2P_SENDPACKET_API_LATEST;
	Options.LocalUserId = Self;
	Options.RemoteUserId = To;
	Options.SocketId = &Socket;
	Options.Channel = Channel;
	Options.DataLengthBytes = static_cast<uint32_t>(Size);
	Options.Data = Data;
	Options.bAllowDelayedDelivery = EOS_TRUE; // the first packets queue up while the connection opens
	Options.Reliability = Reliability;
	Options.bDisableAutoAcceptConnection = EOS_FALSE;
	EOS_P2P_SendPacket(P2P, &Options);
}

void FFightEosTransport::send(uint8_t Peer, const std::vector<uint8_t>& Packet)
{
	if (!P2P || !Peers.IsValidIndex(Peer) || !Peers[Peer].Id || Packet.empty()) return;
	const EOS_ProductUserId To = Peers[Peer].Id;
	const int32 Size = static_cast<int32>(Packet.size());
	if (Size <= MaxEosPacket)
	{
		SendRaw(To, Packet.data(), Size, ChannelMatch, EOS_EPacketReliability::EOS_PR_UnreliableUnordered);
		return;
	}
	const int32 Count = FMath::DivideAndRoundUp(Size, PieceSize);
	if (Count > 255) return; // nothing we send is anywhere near this big
	uint8 Piece[MaxEosPacket];
	for (int32 i = 0; i < Count; ++i)
	{
		const int32 Offset = i * PieceSize;
		const int32 Bytes = FMath::Min(PieceSize, Size - Offset);
		Piece[0] = static_cast<uint8>(i);
		Piece[1] = static_cast<uint8>(Count);
		FMemory::Memcpy(Piece + PieceHeader, Packet.data() + Offset, Bytes);
		SendRaw(To, Piece, Bytes + PieceHeader, ChannelPieces, EOS_EPacketReliability::EOS_PR_ReliableOrdered);
	}
}

bool FFightEosTransport::receive(uint8_t& Peer, std::vector<uint8_t>& Packet)
{
	if (!P2P) return false;
	for (;;)
	{
		EOS_P2P_GetNextReceivedPacketSizeOptions SizeOptions = {};
		SizeOptions.ApiVersion = EOS_P2P_GETNEXTRECEIVEDPACKETSIZE_API_LATEST;
		SizeOptions.LocalUserId = Self;
		SizeOptions.RequestedChannel = nullptr;
		uint32_t Size = 0;
		if (EOS_P2P_GetNextReceivedPacketSize(P2P, &SizeOptions, &Size) != EOS_EResult::EOS_Success) return false;

		EOS_P2P_ReceivePacketOptions Options = {};
		Options.ApiVersion = EOS_P2P_RECEIVEPACKET_API_LATEST;
		Options.LocalUserId = Self;
		Options.MaxDataSizeBytes = static_cast<uint32_t>(Buffer.Num());
		Options.RequestedChannel = nullptr;
		EOS_ProductUserId From = nullptr;
		EOS_P2P_SocketId FromSocket = {};
		uint8_t Channel = 0;
		uint32_t Read = 0;
		if (EOS_P2P_ReceivePacket(P2P, &Options, &From, &FromSocket, &Channel, Buffer.GetData(), &Read) != EOS_EResult::EOS_Success)
			return false;

		const FString Key = KeyOf(From);
		int32 Id = FindPeer(Key);
		if (Id == INDEX_NONE)
		{
			if (!bAcceptNewPeers || Peers.Num() >= fc::net::kMaxPeers || Key.IsEmpty()) continue;
			Id = Peers.Num();
			FPeer& New = Peers.AddDefaulted_GetRef();
			New.Id = From;
			New.Key = Key;
			UE_LOG(LogFight, Log, TEXT("New EOS peer %d (%s)"), Id, *Key);
		}

		if (Channel != ChannelPieces)
		{
			Peer = static_cast<uint8_t>(Id);
			Packet.assign(Buffer.GetData(), Buffer.GetData() + Read);
			return true;
		}
		// one piece of a big packet, they arrive in order
		if (Read < static_cast<uint32_t>(PieceHeader)) continue;
		FPeer& P = Peers[Id];
		const uint8 Index = Buffer[0], Count = Buffer[1];
		if (Index == 0) P.Partial.Reset();
		P.Partial.Append(Buffer.GetData() + PieceHeader, static_cast<int32>(Read) - PieceHeader);
		if (Index + 1 < Count) continue;
		Peer = static_cast<uint8_t>(Id);
		Packet.assign(P.Partial.GetData(), P.Partial.GetData() + P.Partial.Num());
		P.Partial.Reset();
		return true;
	}
}

// ---------------------------------------------------------------- session

FFightEos::FFightEos()
{
	GLive.Add(this);
}

FFightEos::~FFightEos()
{
	Shutdown();
	GLive.Remove(this);
}

FFightEos* FFightEos::Alive(void* ClientData)
{
	return GLive.Contains(ClientData) ? static_cast<FFightEos*>(ClientData) : nullptr;
}

void FFightEos::Fail(const FString& Why)
{
	Error = Why;
	Step = EStep::Failed;
	UE_LOG(LogFight, Error, TEXT("EOS: %s"), *Why);
}

void FFightEos::Begin(const FConfig& InConfig)
{
	Config = InConfig;
	if (Config.ProductId.IsEmpty() || Config.ClientId.IsEmpty())
	{
		Fail(TEXT("EOS isn't set up yet: put the product, sandbox, deployment and client ids into Project Settings > Game > Online"));
		return;
	}
	IEOSSDKManager* Sdk = IEOSSDKManager::Get();
	if (!Sdk)
	{
		Fail(TEXT("the EOSShared plugin isn't loaded"));
		return;
	}
	if (!Sdk->IsInitialized())
	{
		const EOS_EResult Init = Sdk->Initialize();
		if (Init != EOS_EResult::EOS_Success && Init != EOS_EResult::EOS_AlreadyConfigured)
		{
			Fail(FString::Printf(TEXT("SDK init failed (%s)"), *ResultText(Init)));
			return;
		}
	}

	ProductId = Utf8(Config.ProductId);
	SandboxId = Utf8(Config.SandboxId);
	DeploymentId = Utf8(Config.DeploymentId);
	ClientId = Utf8(Config.ClientId);
	ClientSecret = Utf8(Config.ClientSecret);
	CacheDir = Utf8(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("EOS")));
	DisplayName = Utf8(Config.DisplayName.Left(32));
	Bucket = Utf8(Config.Bucket);

	EOS_Platform_Options Options = {};
	Options.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;
	Options.ProductId = ProductId.c_str();
	Options.SandboxId = SandboxId.c_str();
	Options.DeploymentId = DeploymentId.c_str();
	Options.ClientCredentials.ClientId = ClientId.c_str();
	Options.ClientCredentials.ClientSecret = ClientSecret.c_str();
	Options.bIsServer = EOS_FALSE;
	Options.CacheDirectory = CacheDir.c_str();
	// no Epic account overlay, we only use Connect (works without any account)
	Options.Flags = EOS_PF_DISABLE_OVERLAY;
	Platform = Sdk->CreatePlatform(Options);
	Handle = Platform.IsValid() ? static_cast<EOS_HPlatform>(*Platform) : nullptr;
	if (!Handle)
	{
		Fail(TEXT("couldn't create the EOS platform, check the ids"));
		return;
	}
	Login();
}

// Device id login: no account, no password, the id sticks to this install.
// Consoles log in with their platform token instead (PSN, Xbox, Nintendo), which comes from
// the platform SDKs and only builds on dev kits.
void FFightEos::Login()
{
	Step = EStep::LoggingIn;
	EOS_Connect_CreateDeviceIdOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
	Options.DeviceModel = "FightGame";
	EOS_Connect_CreateDeviceId(EOS_Platform_GetConnectInterface(Handle), &Options, this, &FFightEos::OnDeviceId);
}

void EOS_CALL FFightEos::OnDeviceId(const EOS_Connect_CreateDeviceIdCallbackInfo* Data)
{
	FFightEos* Self = Alive(Data->ClientData);
	if (!Self) return;
	// "duplicate" means this install already has one, that's fine
	if (Data->ResultCode != EOS_EResult::EOS_Success && Data->ResultCode != EOS_EResult::EOS_DuplicateNotAllowed)
	{
		Self->Fail(FString::Printf(TEXT("device id failed (%s)"), *ResultText(Data->ResultCode)));
		return;
	}
	EOS_Connect_Credentials Credentials = {};
	Credentials.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
	Credentials.Type = EOS_EExternalCredentialType::EOS_ECT_DEVICEID_ACCESS_TOKEN;
	Credentials.Token = nullptr;
	EOS_Connect_UserLoginInfo Info = {};
	Info.ApiVersion = EOS_CONNECT_USERLOGININFO_API_LATEST;
	Info.DisplayName = Self->DisplayName.c_str();
	EOS_Connect_LoginOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_LOGIN_API_LATEST;
	Options.Credentials = &Credentials;
	Options.UserLoginInfo = &Info;
	EOS_Connect_Login(EOS_Platform_GetConnectInterface(Self->Handle), &Options, Self, &FFightEos::OnLogin);
}

void EOS_CALL FFightEos::OnLogin(const EOS_Connect_LoginCallbackInfo* Data)
{
	FFightEos* Self = Alive(Data->ClientData);
	if (!Self || !EOS_EResult_IsOperationComplete(Data->ResultCode)) return;
	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		Self->LoggedIn(Data->LocalUserId);
		return;
	}
	if (Data->ResultCode == EOS_EResult::EOS_InvalidUser)
	{
		// first time on this device: make a product user for it
		EOS_Connect_CreateUserOptions Options = {};
		Options.ApiVersion = EOS_CONNECT_CREATEUSER_API_LATEST;
		Options.ContinuanceToken = Data->ContinuanceToken;
		EOS_Connect_CreateUser(EOS_Platform_GetConnectInterface(Self->Handle), &Options, Self, &FFightEos::OnUserCreated);
		return;
	}
	Self->Fail(FString::Printf(TEXT("login failed (%s)"), *ResultText(Data->ResultCode)));
}

void EOS_CALL FFightEos::OnUserCreated(const EOS_Connect_CreateUserCallbackInfo* Data)
{
	FFightEos* Self = Alive(Data->ClientData);
	if (!Self) return;
	if (Data->ResultCode != EOS_EResult::EOS_Success)
	{
		Self->Fail(FString::Printf(TEXT("couldn't create a user (%s)"), *ResultText(Data->ResultCode)));
		return;
	}
	Self->LoggedIn(Data->LocalUserId);
}

void FFightEos::LoggedIn(EOS_ProductUserId User)
{
	Me = User;
	UE_LOG(LogFight, Log, TEXT("EOS: logged in as %s"), *FFightEosTransport::KeyOf(Me));
	Transport = MakeUnique<FFightEosTransport>();
	Transport->Begin(EOS_Platform_GetP2PInterface(Handle), Me);
	if (Config.bHost) CreateLobby();
	else Search();
}

void FFightEos::CreateLobby()
{
	Step = EStep::CreatingLobby;
	EOS_Lobby_CreateLobbyOptions Options = {};
	Options.ApiVersion = EOS_LOBBY_CREATELOBBY_API_LATEST;
	Options.LocalUserId = Me;
	Options.MaxLobbyMembers = static_cast<uint32_t>(FMath::Clamp(Config.MaxMembers, 2, fc::net::kMaxPeers));
	Options.PermissionLevel = EOS_ELobbyPermissionLevel::EOS_LPL_PUBLICADVERTISED;
	Options.bPresenceEnabled = EOS_FALSE;
	Options.bAllowInvites = EOS_TRUE;
	Options.BucketId = Bucket.c_str();
	Options.bDisableHostMigration = EOS_TRUE; // the host only matters until the match starts
	Options.bEnableRTCRoom = EOS_FALSE;
	Options.bEnableJoinById = EOS_TRUE;       // lobby codes
	EOS_Lobby_CreateLobby(EOS_Platform_GetLobbyInterface(Handle), &Options, this, &FFightEos::OnLobbyCreated);
}

void EOS_CALL FFightEos::OnLobbyCreated(const EOS_Lobby_CreateLobbyCallbackInfo* Data)
{
	FFightEos* Self = Alive(Data->ClientData);
	if (!Self) return;
	if (Data->ResultCode != EOS_EResult::EOS_Success)
	{
		Self->Fail(FString::Printf(TEXT("couldn't create a lobby (%s)"), *ResultText(Data->ResultCode)));
		return;
	}
	Self->LobbyId = UTF8_TO_TCHAR(Data->LobbyId);
	Self->Transport->SetDescription(FString::Printf(TEXT("lobby code %s"), *Self->LobbyId));
	Self->Transport->SetAcceptNewPeers(true);
	Self->Step = EStep::Ready;
	UE_LOG(LogFight, Log, TEXT("EOS: lobby %s is open"), *Self->LobbyId);
}

void FFightEos::Search()
{
	Step = EStep::Searching;
	EOS_HLobby Lobby = EOS_Platform_GetLobbyInterface(Handle);
	if (SearchHandle) EOS_LobbySearch_Release(SearchHandle);
	SearchHandle = nullptr;

	EOS_Lobby_CreateLobbySearchOptions Create = {};
	Create.ApiVersion = EOS_LOBBY_CREATELOBBYSEARCH_API_LATEST;
	Create.MaxResults = 20;
	if (EOS_Lobby_CreateLobbySearch(Lobby, &Create, &SearchHandle) != EOS_EResult::EOS_Success)
	{
		Fail(TEXT("couldn't start a lobby search"));
		return;
	}

	const std::string Code = Utf8(Config.LobbyCode);
	if (!Code.empty())
	{
		EOS_LobbySearch_SetLobbyIdOptions ById = {};
		ById.ApiVersion = EOS_LOBBYSEARCH_SETLOBBYID_API_LATEST;
		ById.LobbyId = Code.c_str();
		EOS_LobbySearch_SetLobbyId(SearchHandle, &ById);
	}
	else
	{
		EOS_Lobby_AttributeData Attribute = {};
		Attribute.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
		Attribute.Key = EOS_LOBBY_SEARCH_BUCKET_ID;
		Attribute.Value.AsUtf8 = Bucket.c_str();
		Attribute.ValueType = EOS_EAttributeType::EOS_AT_STRING;
		EOS_LobbySearch_SetParameterOptions Parameter = {};
		Parameter.ApiVersion = EOS_LOBBYSEARCH_SETPARAMETER_API_LATEST;
		Parameter.Parameter = &Attribute;
		Parameter.ComparisonOp = EOS_EComparisonOp::EOS_CO_EQUAL;
		EOS_LobbySearch_SetParameter(SearchHandle, &Parameter);
	}

	EOS_LobbySearch_FindOptions Find = {};
	Find.ApiVersion = EOS_LOBBYSEARCH_FIND_API_LATEST;
	Find.LocalUserId = Me;
	EOS_LobbySearch_Find(SearchHandle, &Find, this, &FFightEos::OnSearchDone);
}

void EOS_CALL FFightEos::OnSearchDone(const EOS_LobbySearch_FindCallbackInfo* Data)
{
	FFightEos* Self = Alive(Data->ClientData);
	if (!Self || !Self->SearchHandle) return;
	Self->RetryTimer = SearchRetrySeconds;
	if (Data->ResultCode != EOS_EResult::EOS_Success) return; // "not found" lands here too, just look again

	EOS_LobbySearch_GetSearchResultCountOptions CountOptions = {};
	CountOptions.ApiVersion = EOS_LOBBYSEARCH_GETSEARCHRESULTCOUNT_API_LATEST;
	const uint32_t Count = EOS_LobbySearch_GetSearchResultCount(Self->SearchHandle, &CountOptions);
	for (uint32_t i = 0; i < Count; ++i)
	{
		EOS_LobbySearch_CopySearchResultByIndexOptions CopyOptions = {};
		CopyOptions.ApiVersion = EOS_LOBBYSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST;
		CopyOptions.LobbyIndex = i;
		EOS_HLobbyDetails Details = nullptr;
		if (EOS_LobbySearch_CopySearchResultByIndex(Self->SearchHandle, &CopyOptions, &Details) != EOS_EResult::EOS_Success) continue;

		EOS_LobbyDetails_CopyInfoOptions InfoOptions = {};
		InfoOptions.ApiVersion = EOS_LOBBYDETAILS_COPYINFO_API_LATEST;
		EOS_LobbyDetails_Info* Info = nullptr;
		bool bUsable = false;
		if (EOS_LobbyDetails_CopyInfo(Details, &InfoOptions, &Info) == EOS_EResult::EOS_Success && Info)
		{
			bUsable = Info->AvailableSlots > 0;
			Self->PendingHost = FFightEosTransport::KeyOf(Info->LobbyOwnerUserId);
			EOS_LobbyDetails_Info_Release(Info);
		}
		if (!bUsable || Self->PendingHost.IsEmpty())
		{
			EOS_LobbyDetails_Release(Details);
			continue;
		}

		Self->Step = EStep::JoiningLobby;
		Self->PendingJoin = Details;
		EOS_Lobby_JoinLobbyOptions Join = {};
		Join.ApiVersion = EOS_LOBBY_JOINLOBBY_API_LATEST;
		Join.LobbyDetailsHandle = Details;
		Join.LocalUserId = Self->Me;
		Join.bPresenceEnabled = EOS_FALSE;
		EOS_Lobby_JoinLobby(EOS_Platform_GetLobbyInterface(Self->Handle), &Join, Self, &FFightEos::OnJoined);
		return;
	}
}

void EOS_CALL FFightEos::OnJoined(const EOS_Lobby_JoinLobbyCallbackInfo* Data)
{
	FFightEos* Self = Alive(Data->ClientData);
	if (!Self) return;
	if (Self->PendingJoin) EOS_LobbyDetails_Release(Self->PendingJoin);
	Self->PendingJoin = nullptr;
	if (Data->ResultCode != EOS_EResult::EOS_Success)
	{
		// full or gone in the meantime, keep looking
		UE_LOG(LogFight, Log, TEXT("EOS: join failed (%s), searching again"), *ResultText(Data->ResultCode));
		Self->Step = EStep::Searching;
		Self->RetryTimer = 1.f;
		return;
	}
	Self->LobbyId = UTF8_TO_TCHAR(Data->LobbyId);
	// the host is peer 0, everyone else comes with the Start packet
	Self->Transport->SetPeer(0, Self->PendingHost);
	Self->Transport->SetDescription(FString::Printf(TEXT("lobby code %s"), *Self->LobbyId));
	Self->Step = EStep::Ready;
	UE_LOG(LogFight, Log, TEXT("EOS: joined lobby %s"), *Self->LobbyId);
}

void FFightEos::Tick(float DeltaSeconds)
{
	// the SDK manager ticks the platform itself, we only retry searches
	if (Step != EStep::Searching || RetryTimer <= 0.f) return;
	RetryTimer -= DeltaSeconds;
	if (RetryTimer <= 0.f) Search();
}

void FFightEos::LockLobby()
{
	if (!Config.bHost || LobbyId.IsEmpty() || !Handle) return;
	EOS_HLobby Lobby = EOS_Platform_GetLobbyInterface(Handle);
	const std::string Id = Utf8(LobbyId);
	EOS_Lobby_UpdateLobbyModificationOptions ModOptions = {};
	ModOptions.ApiVersion = EOS_LOBBY_UPDATELOBBYMODIFICATION_API_LATEST;
	ModOptions.LocalUserId = Me;
	ModOptions.LobbyId = Id.c_str();
	EOS_HLobbyModification Modification = nullptr;
	if (EOS_Lobby_UpdateLobbyModification(Lobby, &ModOptions, &Modification) != EOS_EResult::EOS_Success) return;

	EOS_LobbyModification_SetPermissionLevelOptions Permission = {};
	Permission.ApiVersion = EOS_LOBBYMODIFICATION_SETPERMISSIONLEVEL_API_LATEST;
	Permission.PermissionLevel = EOS_ELobbyPermissionLevel::EOS_LPL_INVITEONLY;
	EOS_LobbyModification_SetPermissionLevel(Modification, &Permission);

	EOS_Lobby_UpdateLobbyOptions Update = {};
	Update.ApiVersion = EOS_LOBBY_UPDATELOBBY_API_LATEST;
	Update.LobbyModificationHandle = Modification;
	EOS_Lobby_UpdateLobby(Lobby, &Update, this, &FFightEos::OnLobbyUpdated);
	EOS_LobbyModification_Release(Modification);
}

void EOS_CALL FFightEos::OnLobbyUpdated(const EOS_Lobby_UpdateLobbyCallbackInfo* Data)
{
	if (Data->ResultCode != EOS_EResult::EOS_Success)
		UE_LOG(LogFight, Warning, TEXT("EOS: couldn't lock the lobby (%s)"), *ResultText(Data->ResultCode));
}

void EOS_CALL FFightEos::OnLeft(const EOS_Lobby_LeaveLobbyCallbackInfo*)
{
	// nothing to do, we're on our way out anyway
}

void FFightEos::Shutdown()
{
	if (Handle && Me && !LobbyId.IsEmpty())
	{
		// best effort, EOS also drops members that stop answering
		const std::string Id = Utf8(LobbyId);
		EOS_Lobby_LeaveLobbyOptions Leave = {};
		Leave.ApiVersion = EOS_LOBBY_LEAVELOBBY_API_LATEST;
		Leave.LocalUserId = Me;
		Leave.LobbyId = Id.c_str();
		EOS_Lobby_LeaveLobby(EOS_Platform_GetLobbyInterface(Handle), &Leave, nullptr, &FFightEos::OnLeft);
	}
	LobbyId.Reset();
	if (SearchHandle) EOS_LobbySearch_Release(SearchHandle);
	SearchHandle = nullptr;
	if (PendingJoin) EOS_LobbyDetails_Release(PendingJoin);
	PendingJoin = nullptr;
	Transport.Reset();
	Platform.Reset();
	Handle = nullptr;
	Me = nullptr;
	if (Step != EStep::Failed) Step = EStep::Off;
}

FString FFightEos::GetStatus() const
{
	switch (Step)
	{
	case EStep::LoggingIn: return TEXT("Logging in ...");
	case EStep::Searching: return TEXT("Looking for a lobby ...");
	case EStep::CreatingLobby: return TEXT("Opening a lobby ...");
	case EStep::JoiningLobby: return TEXT("Joining ...");
	case EStep::Ready: return Transport ? Transport->Describe() : FString();
	case EStep::Failed: return FString::Printf(TEXT("Online error: %s"), *Error);
	default: return FString();
	}
}
