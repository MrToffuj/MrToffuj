#pragma once

#include "CoreMinimal.h"
#include "fightcore/Net.h"

class FSocket;
class FInternetAddr;

// What the director needs from a connection on top of fc::net::Transport.
// Addresses are whatever the backend uses to find a console: "ip:port" for UDP,
// a product user id for EOS. They travel inside the Start packet, so the netcode
// never has to know which backend is running.
class FIGHTGAME_API IFightTransport : public fc::net::Transport
{
public:
	// Host only, while collecting players: unknown senders get the next free peer id.
	virtual void SetAcceptNewPeers(bool bAccept) = 0;
	virtual bool SetPeer(uint8 Id, const FString& Address) = 0;
	// index = peer id, slot 0 (the host) stays empty
	virtual TArray<FString> GetPeerAddresses() const = 0;
	// For the lobby screen: "port 7777", "EOS lobby 3fa9..."
	virtual FString Describe() const = 0;
};

// Plain UDP for fc::net. Peer ids are slots in a table of addresses.
//
// The host doesn't know anyone up front, so it accepts new senders and hands out ids in
// arrival order. Clients only talk to the host until the Start packet tells them where
// everyone else is, then they fill in the rest of the table (full mesh).
//
// This is enough for LAN and port-forwarded games. Over the internet use EOS
// (FightEos.h), which brings NAT punch-through and relays.
class FIGHTGAME_API FFightUdpTransport final : public IFightTransport
{
public:
	~FFightUdpTransport();

	// Binds every interface. Port 0 picks a free one (fine for clients).
	bool Open(int32 Port);
	void Close();
	bool IsOpen() const { return Socket != nullptr; }

	virtual void SetAcceptNewPeers(bool bAccept) override { bAcceptNewPeers = bAccept; }
	// "1.2.3.4:7777". Returns false if the address doesn't parse.
	virtual bool SetPeer(uint8 Id, const FString& Address) override;
	virtual TArray<FString> GetPeerAddresses() const override;
	virtual FString Describe() const override { return FString::Printf(TEXT("port %d"), GetBoundPort()); }
	FString GetPeerAddress(uint8 Id) const;
	int32 GetBoundPort() const;

	virtual void send(uint8_t Peer, const std::vector<uint8_t>& Packet) override;
	virtual bool receive(uint8_t& Peer, std::vector<uint8_t>& Packet) override;

	uint64 GetBytesSent() const { return BytesSent; }
	uint64 GetBytesReceived() const { return BytesReceived; }

private:
	int32 FindPeer(const FInternetAddr& Address) const;

	FSocket* Socket = nullptr;
	TArray<TSharedPtr<FInternetAddr>> Peers; // index = peer id
	TArray<uint8> Buffer;
	bool bAcceptNewPeers = false;
	uint64 BytesSent = 0;
	uint64 BytesReceived = 0;
};
