#ifndef GRSIM_REF_GC_ADAPTER_H
#define GRSIM_REF_GC_ADAPTER_H

#include "grsim_core/world_state.h"
#include "grsim_ref/events.h"
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <functional>

// Forward-declare Qt socket to keep the header lightweight
class QTcpSocket;

namespace grsim_ref {

// Referee command from game-controller, matching ssl-game-controller command enum
enum class RefereeCommand {
    HALT = 0,
    STOP = 1,
    NORMAL_START = 2,
    FORCE_START = 3,
    PREPARE_KICKOFF_YELLOW = 4,
    PREPARE_KICKOFF_BLUE = 5,
    PREPARE_PENALTY_YELLOW = 6,
    PREPARE_PENALTY_BLUE = 7,
    DIRECT_FREE_YELLOW = 8,
    DIRECT_FREE_BLUE = 9,
    TIMEOUT_YELLOW = 12,
    TIMEOUT_BLUE = 13,
    BALL_PLACEMENT_YELLOW = 16,
    BALL_PLACEMENT_BLUE = 17,
};

// High-level game stage from game-controller
enum class GameStage {
    NORMAL_FIRST_HALF_PRE = 0,
    NORMAL_FIRST_HALF = 1,
    NORMAL_HALF_TIME = 2,
    NORMAL_SECOND_HALF_PRE = 3,
    NORMAL_SECOND_HALF = 4,
    EXTRA_TIME_BREAK = 5,
    EXTRA_FIRST_HALF_PRE = 6,
    EXTRA_FIRST_HALF = 7,
    EXTRA_HALF_TIME = 8,
    EXTRA_SECOND_HALF_PRE = 9,
    EXTRA_SECOND_HALF = 10,
    PENALTY_SHOOTOUT_BREAK = 11,
    PENALTY_SHOOTOUT = 12,
    POST_GAME = 13,
};

// Simplified running state derived from command + stage
enum class GameState {
    HALT,       // All robots must stop
    STOP,       // Robots must keep distance from ball, slow speed
    RUNNING,    // Normal play
    KICKOFF,    // Preparing or executing kickoff
    FREE_KICK,  // Preparing or executing free kick
    PENALTY,    // Preparing or executing penalty
    BALL_PLACEMENT, // Ball placement in progress
    TIMEOUT,    // Timeout active
    POST_GAME,  // Game over
};

// Parsed referee message from CiOutput
struct RefereeState {
    RefereeCommand command = RefereeCommand::HALT;
    GameStage stage = GameStage::NORMAL_FIRST_HALF_PRE;
    GameState state = GameState::HALT;

    int64_t command_timestamp_us = 0;
    uint32_t command_counter = 0;

    // Score
    int blue_score = 0;
    int yellow_score = 0;

    // Yellow/red cards
    int blue_yellow_cards = 0;
    int yellow_yellow_cards = 0;
    int blue_red_cards = 0;
    int yellow_red_cards = 0;

    // Ball placement target (when command is BALL_PLACEMENT_*)
    double placement_x = 0.0;
    double placement_y = 0.0;
    bool has_placement_pos = false;

    // Game events from this CI cycle
    std::vector<GameEvent> new_events;
};

// Configuration for the game-controller CI adapter
struct GCAdapterConfig {
    std::string host = "127.0.0.1";
    int port = 10009;
    int connect_timeout_ms = 3000;
    int read_timeout_ms = 100;
    bool offline_mode = false;  // If true, never connect; return default state
};

// Game-Controller CI adapter.
//
// Connects to the ssl-game-controller running in CI mode on TCP port 10009.
// Each exchange is:
//   1. Send CiInput (varint-delimited protobuf): timestamp + TrackerWrapperPacket
//   2. Receive CiOutput (varint-delimited protobuf): referee message
//
// In offline/mock mode, returns a configurable default state without connecting.
class GCAdapter {
public:
    explicit GCAdapter(const GCAdapterConfig& config = {});
    ~GCAdapter();

    // Non-copyable, movable
    GCAdapter(const GCAdapter&) = delete;
    GCAdapter& operator=(const GCAdapter&) = delete;
    GCAdapter(GCAdapter&&) noexcept;
    GCAdapter& operator=(GCAdapter&&) noexcept;

    // Connect to the game-controller. Returns true on success.
    // In offline mode, always returns true without connecting.
    bool connect();

    // Disconnect from the game-controller.
    void disconnect();

    // Check if connected (or in offline mode).
    bool isConnected() const;

    // Send world state to game-controller and receive referee state.
    // Converts WorldState to TrackerWrapperPacket, wraps in CiInput,
    // sends over TCP, and parses the CiOutput response.
    //
    // In offline mode, returns the current offline_state_ without network I/O.
    // Returns false on communication error.
    bool exchange(const grsim_core::WorldState& world, RefereeState& out);

    // Convenience: get the current game state from the last exchange
    GameState currentGameState() const { return last_state_.state; }
    RefereeCommand currentCommand() const { return last_state_.command; }
    const RefereeState& lastRefereeState() const { return last_state_; }

    // Offline mode control
    void setOfflineMode(bool enabled);
    bool isOfflineMode() const { return config_.offline_mode; }

    // Set the state returned in offline mode (for testing)
    void setOfflineState(const RefereeState& state) { offline_state_ = state; }

    // Error info
    const std::string& lastError() const { return last_error_; }

private:
    // Build a CiInput protobuf from world state, serialize with varint length prefix
    std::vector<uint8_t> buildCiInput(const grsim_core::WorldState& world);

    // Parse a CiOutput protobuf into RefereeState
    bool parseCiOutput(const std::vector<uint8_t>& data, RefereeState& out);

    // Send length-delimited protobuf message over TCP
    bool sendMessage(const std::vector<uint8_t>& data);

    // Receive length-delimited protobuf message from TCP
    bool receiveMessage(std::vector<uint8_t>& data);

    // Encode/decode varint for protobuf length-delimited framing
    static std::vector<uint8_t> encodeVarint(uint32_t value);
    static bool decodeVarint(const uint8_t* data, size_t len, uint32_t& value, size_t& bytes_read);

    // Map RefereeCommand + GameStage to simplified GameState
    static GameState deriveGameState(RefereeCommand cmd, GameStage stage);

    GCAdapterConfig config_;
    QTcpSocket* socket_ = nullptr;
    RefereeState last_state_;
    RefereeState offline_state_;  // State returned when offline
    std::string last_error_;
};

} // namespace grsim_ref

#endif // GRSIM_REF_GC_ADAPTER_H
