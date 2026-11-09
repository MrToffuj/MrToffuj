#pragma once
// In-memory network for netcode tests: per-packet latency, jitter and loss, fully deterministic.
#include <cstdint>
#include <deque>
#include <vector>

#include "fightcore/Net.h"

struct FakeNetwork {
    struct Packet {
        int64_t deliverAt;
        uint8_t from, to;
        std::vector<uint8_t> data;
    };
    int latency = 3, jitter = 0, lossPct = 0;
    int64_t now = 0;
    uint32_t rng = 12345;
    std::vector<bool> cut; // peers whose packets vanish (disconnects)
    std::deque<Packet> inFlight;
    uint64_t sent = 0, bytes = 0;

    uint32_t next() {
        rng = rng * 1664525u + 1013904223u;
        return rng >> 8;
    }
    void send(uint8_t from, uint8_t to, const std::vector<uint8_t>& data) {
        ++sent;
        bytes += data.size();
        if (from < cut.size() && cut[from]) return;
        if (lossPct > 0 && static_cast<int>(next() % 100) < lossPct) return;
        const int delay = latency + (jitter > 0 ? static_cast<int>(next() % static_cast<uint32_t>(jitter + 1)) : 0);
        inFlight.push_back({now + delay, from, to, data});
    }
    bool receive(uint8_t to, uint8_t& from, std::vector<uint8_t>& out) {
        for (auto it = inFlight.begin(); it != inFlight.end(); ++it) {
            if (it->to != to || it->deliverAt > now) continue;
            from = it->from;
            out = std::move(it->data);
            inFlight.erase(it);
            return true;
        }
        return false;
    }
};

struct FakeTransport : fc::net::Transport {
    FakeNetwork& net;
    uint8_t self;
    FakeTransport(FakeNetwork& n, uint8_t id) : net(n), self(id) {}
    void send(uint8_t peer, const std::vector<uint8_t>& packet) override { net.send(self, peer, packet); }
    bool receive(uint8_t& peer, std::vector<uint8_t>& packet) override { return net.receive(self, peer, packet); }
};
