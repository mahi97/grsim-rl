#include "grsim_ref/gc_adapter.h"

#include <QTcpSocket>
#include <QByteArray>
#include <cstring>
#include <chrono>

// We build the protobuf messages manually (raw field encoding) to avoid
// requiring the full ssl-game-controller proto dependency at build time.
// This keeps the build self-contained while still speaking the CI wire format.
//
// Wire format reference (proto field numbers from ssl-game-controller):
//
// CiInput {
//   uint64 timestamp      = 1;
//   TrackerWrapperPacket tracker_packet = 2;
//   repeated CiApiInput  api_inputs     = 3;
// }
//
// TrackerWrapperPacket {
//   string  uuid           = 1;
//   string  source_name    = 2;
//   TrackedFrame frame     = 3;
// }
//
// TrackedFrame {
//   uint32       frame_number = 1;
//   double       timestamp    = 2;
//   repeated TrackedBall   balls  = 3;
//   repeated TrackedRobot  robots = 4;
// }
//
// TrackedBall  { Vector3 pos = 1; Vector3 vel = 2; ... }
// TrackedRobot { RobotId robot_id = 1; Vector3 pos = 2; double orientation = 3;
//                Vector2 vel = 4; double vel_angular = 5; ... }
//
// CiOutput {
//   Referee referee_message = 1;
// }

namespace grsim_ref {

// ============================================================
// Protobuf wire-format helpers (manual encoding)
// ============================================================

namespace proto {

// Wire types
constexpr uint8_t VARINT = 0;
constexpr uint8_t FIXED64 = 1;
constexpr uint8_t LENGTH_DELIMITED = 2;
constexpr uint8_t FIXED32 = 5;

static void appendVarint(std::vector<uint8_t>& buf, uint64_t value) {
    do {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if (value) byte |= 0x80;
        buf.push_back(byte);
    } while (value);
}

static void appendTag(std::vector<uint8_t>& buf, uint32_t field, uint8_t wire_type) {
    appendVarint(buf, (static_cast<uint64_t>(field) << 3) | wire_type);
}

static void appendDouble(std::vector<uint8_t>& buf, uint32_t field, double value) {
    appendTag(buf, field, FIXED64);
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    for (int i = 0; i < 8; ++i) {
        buf.push_back(static_cast<uint8_t>(bits & 0xFF));
        bits >>= 8;
    }
}

static void appendFloat(std::vector<uint8_t>& buf, uint32_t field, float value) {
    appendTag(buf, field, FIXED32);
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    for (int i = 0; i < 4; ++i) {
        buf.push_back(static_cast<uint8_t>(bits & 0xFF));
        bits >>= 8;
    }
}

static void appendUint32(std::vector<uint8_t>& buf, uint32_t field, uint32_t value) {
    appendTag(buf, field, VARINT);
    appendVarint(buf, value);
}

static void appendUint64(std::vector<uint8_t>& buf, uint32_t field, uint64_t value) {
    appendTag(buf, field, VARINT);
    appendVarint(buf, value);
}

static void appendString(std::vector<uint8_t>& buf, uint32_t field, const std::string& value) {
    appendTag(buf, field, LENGTH_DELIMITED);
    appendVarint(buf, value.size());
    buf.insert(buf.end(), value.begin(), value.end());
}

static void appendBytes(std::vector<uint8_t>& buf, uint32_t field, const std::vector<uint8_t>& value) {
    appendTag(buf, field, LENGTH_DELIMITED);
    appendVarint(buf, value.size());
    buf.insert(buf.end(), value.begin(), value.end());
}

// Decode a varint from a byte buffer. Returns false if insufficient data.
static bool readVarint(const uint8_t* data, size_t len, uint64_t& value, size_t& consumed) {
    value = 0;
    consumed = 0;
    for (size_t i = 0; i < len && i < 10; ++i) {
        value |= static_cast<uint64_t>(data[i] & 0x7F) << (7 * i);
        consumed = i + 1;
        if ((data[i] & 0x80) == 0) return true;
    }
    return false;
}

// Read a tag (field number + wire type)
static bool readTag(const uint8_t* data, size_t len, uint32_t& field, uint8_t& wire_type, size_t& consumed) {
    uint64_t v;
    if (!readVarint(data, len, v, consumed)) return false;
    wire_type = v & 0x07;
    field = static_cast<uint32_t>(v >> 3);
    return true;
}

// Skip a field value based on wire type, returns bytes consumed
static bool skipField(const uint8_t* data, size_t len, uint8_t wire_type, size_t& consumed) {
    consumed = 0;
    switch (wire_type) {
        case VARINT: {
            uint64_t v;
            return readVarint(data, len, v, consumed);
        }
        case FIXED64:
            if (len < 8) return false;
            consumed = 8;
            return true;
        case LENGTH_DELIMITED: {
            uint64_t v;
            size_t hdr;
            if (!readVarint(data, len, v, hdr)) return false;
            if (hdr + v > len) return false;
            consumed = hdr + static_cast<size_t>(v);
            return true;
        }
        case FIXED32:
            if (len < 4) return false;
            consumed = 4;
            return true;
        default:
            return false;
    }
}

static double readDoubleValue(const uint8_t* data) {
    uint64_t bits = 0;
    for (int i = 7; i >= 0; --i) {
        bits = (bits << 8) | data[i];
    }
    double v;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

static float readFloatValue(const uint8_t* data) {
    uint32_t bits = 0;
    for (int i = 3; i >= 0; --i) {
        bits = (bits << 8) | data[i];
    }
    float v;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

} // namespace proto

// ============================================================
// TrackerWrapperPacket builder
// ============================================================

// Build Vector3 submessage: { float x = 1; float y = 2; float z = 3; }
static std::vector<uint8_t> buildVector3(float x, float y, float z) {
    std::vector<uint8_t> buf;
    proto::appendFloat(buf, 1, x);
    proto::appendFloat(buf, 2, y);
    proto::appendFloat(buf, 3, z);
    return buf;
}

// Build Vector2 submessage: { float x = 1; float y = 2; }
static std::vector<uint8_t> buildVector2(float x, float y) {
    std::vector<uint8_t> buf;
    proto::appendFloat(buf, 1, x);
    proto::appendFloat(buf, 2, y);
    return buf;
}

// Build RobotId: { uint32 id = 1; Team team = 2; }
static std::vector<uint8_t> buildRobotId(uint32_t id, int team) {
    std::vector<uint8_t> buf;
    proto::appendUint32(buf, 1, id);
    // Team enum: UNKNOWN=0, YELLOW=1, BLUE=2
    proto::appendUint32(buf, 2, (team == 0) ? 2 : 1);
    return buf;
}

// Build TrackedBall: { Vector3 pos = 1; Vector3 vel = 2; optional Visibility visibility = 3; }
static std::vector<uint8_t> buildTrackedBall(const grsim_core::BallState& ball) {
    std::vector<uint8_t> buf;
    // Position in meters (GC expects meters, our world state is in meters)
    auto pos = buildVector3(
        static_cast<float>(ball.x),
        static_cast<float>(ball.y),
        static_cast<float>(ball.z)
    );
    proto::appendBytes(buf, 1, pos);

    auto vel = buildVector3(
        static_cast<float>(ball.vx),
        static_cast<float>(ball.vy),
        static_cast<float>(ball.vz)
    );
    proto::appendBytes(buf, 2, vel);
    return buf;
}

// Build TrackedRobot
static std::vector<uint8_t> buildTrackedRobot(const grsim_core::RobotState& robot) {
    std::vector<uint8_t> buf;

    auto rid = buildRobotId(static_cast<uint32_t>(robot.id), robot.team);
    proto::appendBytes(buf, 1, rid);

    // pos: x, y in meters, z = 0 (ground robots)
    auto pos = buildVector3(
        static_cast<float>(robot.x),
        static_cast<float>(robot.y),
        0.0f
    );
    proto::appendBytes(buf, 2, pos);

    // orientation (float field 3)
    proto::appendFloat(buf, 3, static_cast<float>(robot.orientation));

    // vel: 2D velocity
    auto vel = buildVector2(
        static_cast<float>(robot.vx),
        static_cast<float>(robot.vy)
    );
    proto::appendBytes(buf, 4, vel);

    // vel_angular (float field 5)
    proto::appendFloat(buf, 5, static_cast<float>(robot.vw));

    return buf;
}

// Build TrackedFrame
static std::vector<uint8_t> buildTrackedFrame(const grsim_core::WorldState& world) {
    std::vector<uint8_t> buf;

    proto::appendUint32(buf, 1, static_cast<uint32_t>(world.frame_num));
    proto::appendDouble(buf, 2, world.sim_time);

    // Ball (field 3, repeated but we send one)
    auto ball = buildTrackedBall(world.ball);
    proto::appendBytes(buf, 3, ball);

    // Robots (field 4, repeated)
    for (const auto& r : world.blue_robots) {
        if (!r.present) continue;
        auto robot = buildTrackedRobot(r);
        proto::appendBytes(buf, 4, robot);
    }
    for (const auto& r : world.yellow_robots) {
        if (!r.present) continue;
        auto robot = buildTrackedRobot(r);
        proto::appendBytes(buf, 4, robot);
    }

    return buf;
}

// Build TrackerWrapperPacket
static std::vector<uint8_t> buildTrackerWrapper(const grsim_core::WorldState& world) {
    std::vector<uint8_t> buf;

    proto::appendString(buf, 1, "grsim-rl");       // uuid
    proto::appendString(buf, 2, "grsim-rl-ci");    // source_name

    auto frame = buildTrackedFrame(world);
    proto::appendBytes(buf, 3, frame);

    return buf;
}

// ============================================================
// GCAdapter implementation
// ============================================================

GCAdapter::GCAdapter(const GCAdapterConfig& config)
    : config_(config)
{
    // Set offline_state_ to a reasonable default (HALT)
    offline_state_.command = RefereeCommand::HALT;
    offline_state_.state = GameState::HALT;
    offline_state_.stage = GameStage::NORMAL_FIRST_HALF;
}

GCAdapter::~GCAdapter() {
    disconnect();
}

GCAdapter::GCAdapter(GCAdapter&& other) noexcept
    : config_(std::move(other.config_))
    , socket_(other.socket_)
    , last_state_(std::move(other.last_state_))
    , offline_state_(std::move(other.offline_state_))
    , last_error_(std::move(other.last_error_))
{
    other.socket_ = nullptr;
}

GCAdapter& GCAdapter::operator=(GCAdapter&& other) noexcept {
    if (this != &other) {
        disconnect();
        config_ = std::move(other.config_);
        socket_ = other.socket_;
        last_state_ = std::move(other.last_state_);
        offline_state_ = std::move(other.offline_state_);
        last_error_ = std::move(other.last_error_);
        other.socket_ = nullptr;
    }
    return *this;
}

bool GCAdapter::connect() {
    if (config_.offline_mode) return true;

    if (socket_) {
        disconnect();
    }

    socket_ = new QTcpSocket();
    socket_->connectToHost(QString::fromStdString(config_.host), config_.port);

    if (!socket_->waitForConnected(config_.connect_timeout_ms)) {
        last_error_ = "Failed to connect to game-controller at "
                     + config_.host + ":" + std::to_string(config_.port)
                     + " — " + socket_->errorString().toStdString();
        delete socket_;
        socket_ = nullptr;
        return false;
    }

    last_error_.clear();
    return true;
}

void GCAdapter::disconnect() {
    if (socket_) {
        if (socket_->state() == QTcpSocket::ConnectedState) {
            socket_->disconnectFromHost();
            socket_->waitForDisconnected(1000);
        }
        delete socket_;
        socket_ = nullptr;
    }
}

bool GCAdapter::isConnected() const {
    if (config_.offline_mode) return true;
    return socket_ && socket_->state() == QTcpSocket::ConnectedState;
}

void GCAdapter::setOfflineMode(bool enabled) {
    if (enabled && !config_.offline_mode) {
        disconnect();
    }
    config_.offline_mode = enabled;
}

bool GCAdapter::exchange(const grsim_core::WorldState& world, RefereeState& out) {
    if (config_.offline_mode) {
        out = offline_state_;
        last_state_ = offline_state_;
        return true;
    }

    if (!isConnected()) {
        last_error_ = "Not connected to game-controller";
        return false;
    }

    // Build and send CiInput
    auto ci_input = buildCiInput(world);
    if (!sendMessage(ci_input)) {
        return false;
    }

    // Receive CiOutput
    std::vector<uint8_t> response;
    if (!receiveMessage(response)) {
        return false;
    }

    // Parse CiOutput
    if (!parseCiOutput(response, out)) {
        return false;
    }

    last_state_ = out;
    return true;
}

std::vector<uint8_t> GCAdapter::buildCiInput(const grsim_core::WorldState& world) {
    std::vector<uint8_t> buf;

    // Field 1: timestamp (uint64, nanoseconds)
    auto now_ns = static_cast<uint64_t>(world.sim_time * 1e9);
    proto::appendUint64(buf, 1, now_ns);

    // Field 2: tracker_packet (TrackerWrapperPacket, length-delimited)
    auto tracker = buildTrackerWrapper(world);
    proto::appendBytes(buf, 2, tracker);

    return buf;
}

bool GCAdapter::parseCiOutput(const std::vector<uint8_t>& data, RefereeState& out) {
    // CiOutput { Referee referee_message = 1; }
    // We need to find field 1 (length-delimited) and parse the Referee message inside.
    //
    // Referee message fields we care about:
    //   uint64  packet_timestamp = 1;
    //   Stage   stage            = 2;  (enum/varint)
    //   Command command          = 4;  (optional, enum/varint)
    //   uint32  command_counter  = 5;
    //   uint64  command_timestamp = 6;
    //   TeamInfo yellow           = 7;
    //   TeamInfo blue             = 8;
    //   Point   designated_position = 9;
    //   repeated GameEvent game_events = 16;  (we skip these for now)
    //
    // TeamInfo { string name=1; uint32 score=2; uint32 red_cards=3;
    //            repeated uint32 yellow_card_times=4; uint32 yellow_cards=5; ... }

    const uint8_t* p = data.data();
    size_t remaining = data.size();

    // Find the referee_message (field 1) in CiOutput
    const uint8_t* ref_data = nullptr;
    size_t ref_len = 0;

    while (remaining > 0) {
        uint32_t field;
        uint8_t wire;
        size_t tag_bytes;
        if (!proto::readTag(p, remaining, field, wire, tag_bytes)) break;
        p += tag_bytes;
        remaining -= tag_bytes;

        if (field == 1 && wire == proto::LENGTH_DELIMITED) {
            uint64_t msg_len;
            size_t len_bytes;
            if (!proto::readVarint(p, remaining, msg_len, len_bytes)) break;
            p += len_bytes;
            remaining -= len_bytes;
            ref_data = p;
            ref_len = static_cast<size_t>(msg_len);
            break;
        } else {
            size_t skip;
            if (!proto::skipField(p, remaining, wire, skip)) break;
            p += skip;
            remaining -= skip;
        }
    }

    if (!ref_data) {
        last_error_ = "CiOutput missing referee_message field";
        return false;
    }

    // Parse Referee message
    p = ref_data;
    remaining = ref_len;
    out = RefereeState{};

    while (remaining > 0) {
        uint32_t field;
        uint8_t wire;
        size_t tag_bytes;
        if (!proto::readTag(p, remaining, field, wire, tag_bytes)) break;
        p += tag_bytes;
        remaining -= tag_bytes;

        if (wire == proto::VARINT) {
            uint64_t v;
            size_t vb;
            if (!proto::readVarint(p, remaining, v, vb)) break;
            p += vb;
            remaining -= vb;

            switch (field) {
                case 2: out.stage = static_cast<GameStage>(v); break;
                case 4: out.command = static_cast<RefereeCommand>(v); break;
                case 5: out.command_counter = static_cast<uint32_t>(v); break;
                case 6: out.command_timestamp_us = static_cast<int64_t>(v); break;
                default: break;
            }
        } else if (wire == proto::LENGTH_DELIMITED) {
            uint64_t msg_len;
            size_t len_bytes;
            if (!proto::readVarint(p, remaining, msg_len, len_bytes)) break;
            p += len_bytes;
            remaining -= len_bytes;
            size_t sub_len = static_cast<size_t>(msg_len);

            if (field == 7 || field == 8) {
                // TeamInfo — parse score, cards
                const uint8_t* tp = p;
                size_t tr = sub_len;
                int score = 0, yellow_cards = 0, red_cards = 0;

                while (tr > 0) {
                    uint32_t tf;
                    uint8_t tw;
                    size_t ttb;
                    if (!proto::readTag(tp, tr, tf, tw, ttb)) break;
                    tp += ttb;
                    tr -= ttb;

                    if (tw == proto::VARINT) {
                        uint64_t tv;
                        size_t tvb;
                        if (!proto::readVarint(tp, tr, tv, tvb)) break;
                        tp += tvb;
                        tr -= tvb;
                        switch (tf) {
                            case 2: score = static_cast<int>(tv); break;
                            case 3: red_cards = static_cast<int>(tv); break;
                            case 5: yellow_cards = static_cast<int>(tv); break;
                            default: break;
                        }
                    } else {
                        size_t skip;
                        if (!proto::skipField(tp, tr, tw, skip)) break;
                        tp += skip;
                        tr -= skip;
                    }
                }

                if (field == 7) {  // yellow team info
                    out.yellow_score = score;
                    out.yellow_yellow_cards = yellow_cards;
                    out.yellow_red_cards = red_cards;
                } else {  // blue team info (field 8)
                    out.blue_score = score;
                    out.blue_yellow_cards = yellow_cards;
                    out.blue_red_cards = red_cards;
                }
            } else if (field == 9) {
                // designated_position: Point { float x = 1; float y = 2; }
                const uint8_t* dp = p;
                size_t dr = sub_len;
                out.has_placement_pos = true;

                while (dr > 0) {
                    uint32_t df;
                    uint8_t dw;
                    size_t dtb;
                    if (!proto::readTag(dp, dr, df, dw, dtb)) break;
                    dp += dtb;
                    dr -= dtb;

                    if (dw == proto::FIXED32 && dr >= 4) {
                        float fv = proto::readFloatValue(dp);
                        dp += 4;
                        dr -= 4;
                        if (df == 1) out.placement_x = fv;
                        else if (df == 2) out.placement_y = fv;
                    } else {
                        size_t skip;
                        if (!proto::skipField(dp, dr, dw, skip)) break;
                        dp += skip;
                        dr -= skip;
                    }
                }
            }

            p += sub_len;
            remaining -= sub_len;
        } else if (wire == proto::FIXED64) {
            if (remaining < 8) break;
            p += 8;
            remaining -= 8;
        } else if (wire == proto::FIXED32) {
            if (remaining < 4) break;
            p += 4;
            remaining -= 4;
        } else {
            break;
        }
    }

    out.state = deriveGameState(out.command, out.stage);
    return true;
}

bool GCAdapter::sendMessage(const std::vector<uint8_t>& data) {
    // Varint length prefix + payload
    auto prefix = encodeVarint(static_cast<uint32_t>(data.size()));

    QByteArray packet;
    packet.append(reinterpret_cast<const char*>(prefix.data()), static_cast<int>(prefix.size()));
    packet.append(reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()));

    qint64 written = socket_->write(packet);
    if (written != packet.size()) {
        last_error_ = "Failed to write CiInput to socket";
        return false;
    }
    if (!socket_->waitForBytesWritten(config_.read_timeout_ms)) {
        last_error_ = "Timeout waiting for bytes written";
        return false;
    }
    return true;
}

bool GCAdapter::receiveMessage(std::vector<uint8_t>& data) {
    // Read varint length prefix
    if (!socket_->waitForReadyRead(config_.read_timeout_ms)) {
        last_error_ = "Timeout waiting for CiOutput response";
        return false;
    }

    // Read available bytes, decode varint header
    QByteArray buf = socket_->readAll();
    if (buf.isEmpty()) {
        last_error_ = "Empty response from game-controller";
        return false;
    }

    const auto* raw = reinterpret_cast<const uint8_t*>(buf.constData());
    size_t total = static_cast<size_t>(buf.size());

    uint32_t msg_len;
    size_t hdr_bytes;
    if (!decodeVarint(raw, total, msg_len, hdr_bytes)) {
        last_error_ = "Failed to decode varint length prefix";
        return false;
    }

    size_t needed = hdr_bytes + msg_len;

    // If we don't have the full message yet, keep reading
    while (static_cast<size_t>(buf.size()) < needed) {
        if (!socket_->waitForReadyRead(config_.read_timeout_ms)) {
            last_error_ = "Timeout waiting for complete CiOutput message";
            return false;
        }
        buf.append(socket_->readAll());
    }

    raw = reinterpret_cast<const uint8_t*>(buf.constData());
    data.assign(raw + hdr_bytes, raw + hdr_bytes + msg_len);
    return true;
}

std::vector<uint8_t> GCAdapter::encodeVarint(uint32_t value) {
    std::vector<uint8_t> buf;
    do {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if (value) byte |= 0x80;
        buf.push_back(byte);
    } while (value);
    return buf;
}

bool GCAdapter::decodeVarint(const uint8_t* data, size_t len, uint32_t& value, size_t& bytes_read) {
    uint64_t v;
    bool ok = proto::readVarint(data, len, v, bytes_read);
    value = static_cast<uint32_t>(v);
    return ok;
}

GameState GCAdapter::deriveGameState(RefereeCommand cmd, GameStage stage) {
    if (stage == GameStage::POST_GAME) return GameState::POST_GAME;

    switch (cmd) {
        case RefereeCommand::HALT:
            return GameState::HALT;
        case RefereeCommand::STOP:
            return GameState::STOP;
        case RefereeCommand::NORMAL_START:
        case RefereeCommand::FORCE_START:
            return GameState::RUNNING;
        case RefereeCommand::PREPARE_KICKOFF_YELLOW:
        case RefereeCommand::PREPARE_KICKOFF_BLUE:
            return GameState::KICKOFF;
        case RefereeCommand::PREPARE_PENALTY_YELLOW:
        case RefereeCommand::PREPARE_PENALTY_BLUE:
            return GameState::PENALTY;
        case RefereeCommand::DIRECT_FREE_YELLOW:
        case RefereeCommand::DIRECT_FREE_BLUE:
            return GameState::FREE_KICK;
        case RefereeCommand::TIMEOUT_YELLOW:
        case RefereeCommand::TIMEOUT_BLUE:
            return GameState::TIMEOUT;
        case RefereeCommand::BALL_PLACEMENT_YELLOW:
        case RefereeCommand::BALL_PLACEMENT_BLUE:
            return GameState::BALL_PLACEMENT;
        default:
            return GameState::HALT;
    }
}

} // namespace grsim_ref
