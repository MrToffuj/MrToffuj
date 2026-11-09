#pragma once

#include "CoreMinimal.h"
#include "fightcore/Net.h"

class FSocket;
class FInternetAddr;

// Plain UDP for fc::net. Peer ids are slots in a table of addresses.
//
// The host doesn't know anyone up front, so it accepts new senders and hands out ids in
// arrival order. Clients only talk to the host until the Start packet tells them where
// everyone else is, then they fill in the rest of the table (full mesh).
//
// This is enough for LAN and port-forwarded games. NAT punch-through goes in a second
// Transport on top of a platform relay (EOS P2P, Steam networking), the netcode above
// doesn't care which one it gets.
class FIGHTGAME_API FFightUdpTransport final : public fc::net::Transport
{
public:
	~FFightUdpTransport();

	// Binds every interface. Port 0 picks a free one (fine for clients).
	bool Open(int32 Port);
	void Close();
	bool IsOpen() const { return Socket != nullptr; }

	void SetAcceptNewPeers(bool bAccept) { bAcceptNewPeers = bAccept; }
	// "1.2.3.4:7777". Returns false if the address doesn't parse.
	bool SetPeer(uint8 Id, const FString& Address);
	FString GetPeerAddress(uint8 Id) const;
	int32 GetBoundPort() const;

	// Everything the host has seen, index = peer id. Slot 0 is the host itself and stays empty.
	TArray<FString> GetPeerAddresses() const;

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
