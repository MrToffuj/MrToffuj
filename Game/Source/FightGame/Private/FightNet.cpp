#include "FightNet.h"

#include "FightGame.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

namespace
{
	// Big enough for a full 20 slot lobby. Match packets are a few dozen bytes.
	constexpr int32 MaxPacket = 8192;

	ISocketSubsystem* Sockets() { return ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM); }
}

FFightUdpTransport::~FFightUdpTransport()
{
	Close();
}

bool FFightUdpTransport::Open(int32 Port)
{
	Close();
	ISocketSubsystem* Subsystem = Sockets();
	if (!Subsystem) return false;

	Socket = Subsystem->CreateSocket(NAME_DGram, TEXT("FightP2P"), false);
	if (!Socket) return false;
	Socket->SetNonBlocking(true);
	Socket->SetReuseAddr(true);
	int32 Actual = 0;
	Socket->SetReceiveBufferSize(256 * 1024, Actual);
	Socket->SetSendBufferSize(256 * 1024, Actual);

	TSharedRef<FInternetAddr> Any = Subsystem->CreateInternetAddr();
	Any->SetAnyAddress();
	Any->SetPort(Port);
	if (!Socket->Bind(*Any))
	{
		UE_LOG(LogFight, Error, TEXT("Couldn't bind UDP port %d"), Port);
		Close();
		return false;
	}
	Buffer.SetNumUninitialized(MaxPacket);
	if (Peers.Num() == 0) Peers.SetNum(1);
	UE_LOG(LogFight, Log, TEXT("P2P socket on port %d"), GetBoundPort());
	return true;
}

void FFightUdpTransport::Close()
{
	if (!Socket) return;
	Socket->Close();
	Sockets()->DestroySocket(Socket);
	Socket = nullptr;
}

bool FFightUdpTransport::SetPeer(uint8 Id, const FString& Address)
{
	TSharedPtr<FInternetAddr> Addr = Sockets()->GetAddressFromString(Address);
	if (!Addr.IsValid() || !Addr->IsValid()) return false;
	if (Peers.Num() <= Id) Peers.SetNum(Id + 1);
	Peers[Id] = Addr;
	return true;
}

FString FFightUdpTransport::GetPeerAddress(uint8 Id) const
{
	return Peers.IsValidIndex(Id) && Peers[Id].IsValid() ? Peers[Id]->ToString(true) : FString();
}

int32 FFightUdpTransport::GetBoundPort() const
{
	return Socket ? Socket->GetPortNo() : 0;
}

TArray<FString> FFightUdpTransport::GetPeerAddresses() const
{
	TArray<FString> Out;
	for (int32 i = 0; i < Peers.Num(); ++i) Out.Add(GetPeerAddress(static_cast<uint8>(i)));
	return Out;
}

int32 FFightUdpTransport::FindPeer(const FInternetAddr& Address) const
{
	for (int32 i = 0; i < Peers.Num(); ++i)
		if (Peers[i].IsValid() && Peers[i]->CompareEndpoints(Address)) return i;
	return INDEX_NONE;
}

void FFightUdpTransport::send(uint8_t Peer, const std::vector<uint8_t>& Packet)
{
	if (!Socket || !Peers.IsValidIndex(Peer) || !Peers[Peer].IsValid() || Packet.empty()) return;
	int32 Sent = 0;
	// UDP, so a full send buffer just drops the packet. The netcode resends anyway.
	Socket->SendTo(Packet.data(), static_cast<int32>(Packet.size()), Sent, *Peers[Peer]);
	BytesSent += static_cast<uint64>(FMath::Max(0, Sent));
}

bool FFightUdpTransport::receive(uint8_t& Peer, std::vector<uint8_t>& Packet)
{
	if (!Socket) return false;
	TSharedRef<FInternetAddr> From = Sockets()->CreateInternetAddr();
	uint32 Pending = 0;
	while (Socket->HasPendingData(Pending))
	{
		int32 Read = 0;
		if (!Socket->RecvFrom(Buffer.GetData(), Buffer.Num(), Read, *From) || Read <= 0) return false;
		BytesReceived += static_cast<uint64>(Read);

		int32 Id = FindPeer(*From);
		if (Id == INDEX_NONE)
		{
			// strangers only get in while the host is still collecting players
			if (!bAcceptNewPeers || Peers.Num() >= fc::net::kMaxPeers) continue;
			Id = Peers.Num();
			Peers.Add(From);
			From = Sockets()->CreateInternetAddr();
			UE_LOG(LogFight, Log, TEXT("New peer %d from %s"), Id, *GetPeerAddress(static_cast<uint8>(Id)));
		}
		Peer = static_cast<uint8_t>(Id);
		Packet.assign(Buffer.GetData(), Buffer.GetData() + Read);
		return true;
	}
	return false;
}
