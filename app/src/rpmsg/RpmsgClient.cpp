/**
 * SmartHub RPMsg Client Implementation
 *
 * Communicates with Cortex-M4 via RPMsg TTY device.
 */

#include <smarthub/rpmsg/RpmsgClient.hpp>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/core/Logger.hpp>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <chrono>
#include <cstring>
#include <thread>

namespace smarthub {

RpmsgClient::RpmsgClient(EventBus& eventBus, const std::string& device)
    : m_eventBus(eventBus)
    , m_device(device)
{
}

RpmsgClient::~RpmsgClient() {
    shutdown();
}

bool RpmsgClient::initialize(int pingTimeoutMs) {
    shutdown();  // In case this is a retry

    // Try to open the RPMsg device
    m_fd = open(m_device.c_str(), O_RDWR | O_NONBLOCK);

    if (m_fd < 0) {
        LOG_WARN("Could not open RPMsg device %s: %s",
                 m_device.c_str(), strerror(errno));
        return false;
    }

    // Configure as raw terminal
    struct termios tio;
    if (tcgetattr(m_fd, &tio) == 0) {
        cfmakeraw(&tio);
        tcsetattr(m_fd, TCSANOW, &tio);
    }

    m_connected = true;
    LOG_INFO("RPMsg client connected to %s", m_device.c_str());

    // Writing only shows the device is open. The M4 has answered if a pong comes back.
    m_lastPing = std::chrono::steady_clock::now();
    if (!ping()) {
        return false;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(pingTimeoutMs);
    while (!m_m4Responded && m_connected && std::chrono::steady_clock::now() < deadline) {
        poll();
        if (!m_m4Responded) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    if (!m_m4Responded) {
        LOG_WARN("The M4 didn't answer a ping within %d ms", pingTimeoutMs);
    }
    return m_m4Responded;
}

void RpmsgClient::shutdown() {
    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }
    m_connected = false;
    m_m4Responded = false;
    m_received.clear();
}

void RpmsgClient::poll() {
    if (!m_connected || m_fd < 0) {
        return;
    }

    // It might have started after this did
    constexpr auto kPingInterval = std::chrono::seconds(5);
    const auto now = std::chrono::steady_clock::now();
    if (!m_m4Responded && now - m_lastPing >= kPingInterval) {
        m_lastPing = now;
        ping();
    }

    // Check for available data
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(m_fd, &readfds);

    struct timeval tv = {0, 0}; // Non-blocking
    int ret = select(m_fd + 1, &readfds, nullptr, nullptr, &tv);

    if (ret > 0 && FD_ISSET(m_fd, &readfds)) {
        uint8_t buffer[protocol::MAX_MESSAGE_SIZE];
        ssize_t n = read(m_fd, buffer, sizeof(buffer));

        if (n > 0) {
            handleReceived(buffer, static_cast<size_t>(n));
        } else if (n < 0 && errno != EAGAIN) {
            LOG_ERROR("RPMsg read error: %s", strerror(errno));
            m_connected = false;
        }
    }
}

bool RpmsgClient::send(const std::vector<uint8_t>& data) {
    if (!m_connected || m_fd < 0) {
        return false;
    }

    ssize_t n = write(m_fd, data.data(), data.size());
    if (n < 0) {
        LOG_ERROR("RPMsg write error: %s", strerror(errno));
        return false;
    }

    return static_cast<size_t>(n) == data.size();
}

bool RpmsgClient::sendMessage(protocol::MsgType type, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> message(sizeof(protocol::MsgHeader) + payload.size());
    const size_t size = protocol::encodeMessage(type, m_nextSeq, payload.empty() ? nullptr : payload.data(),
                                                payload.size(), message.data(), message.size());
    if (size == 0) {
        LOG_ERROR("RPMsg message of %zu bytes is too large", payload.size());
        return false;
    }
    if (!send(message)) {
        return false;
    }
    m_nextSeq++;
    return true;
}

bool RpmsgClient::requestSensorData() {
    return sendMessage(protocol::MsgType::CMD_GET_SENSOR_DATA, {});
}

bool RpmsgClient::setSensorInterval(uint16_t intervalMs) {
    protocol::IntervalPayload payload{intervalMs};
    const auto* bytes = reinterpret_cast<const uint8_t*>(&payload);
    return sendMessage(protocol::MsgType::CMD_SET_INTERVAL, {bytes, bytes + sizeof(payload)});
}

bool RpmsgClient::setGpio(uint8_t port, uint8_t pin, bool state) {
    protocol::GPIOPayload payload{port, pin, static_cast<uint8_t>(state ? 1 : 0), 0};
    const auto* bytes = reinterpret_cast<const uint8_t*>(&payload);
    return sendMessage(protocol::MsgType::CMD_SET_GPIO, {bytes, bytes + sizeof(payload)});
}

bool RpmsgClient::ping() {
    return sendMessage(protocol::MsgType::CMD_PING, {});
}

void RpmsgClient::handleReceived(const uint8_t* data, size_t size) {
    // The start of a message that never finished, as when the M4 reset partway, isn't part of this
    constexpr auto kStaleAfter = std::chrono::seconds(1);
    const auto now = std::chrono::steady_clock::now();
    if (!m_received.empty() && now - m_lastReceived > kStaleAfter) {
        LOG_WARN("Dropping %zu bytes of an unfinished message from the M4", m_received.size());
        m_received.clear();
    }
    m_lastReceived = now;
    m_received.insert(m_received.end(), data, data + size);

    // Copied out and removed before anything hears about them, as a handler can call back into this
    std::vector<std::vector<uint8_t>> messages;
    size_t used = 0;
    size_t skipped = 0;
    while (used < m_received.size()) {
        protocol::DecodedMessage message;
        const auto result = protocol::decodeMessage(m_received.data() + used, m_received.size() - used, message);
        if (result == protocol::DecodeResult::NeedMore) {
            break;
        }
        if (result == protocol::DecodeResult::Invalid) {
            // Look for where the next message starts, which may be right behind this
            used++;
            skipped++;
            continue;
        }
        messages.emplace_back(m_received.begin() + static_cast<std::ptrdiff_t>(used),
                              m_received.begin() + static_cast<std::ptrdiff_t>(used + message.size));
        used += message.size;
    }
    m_received.erase(m_received.begin(), m_received.begin() + static_cast<std::ptrdiff_t>(used));
    if (skipped > 0) {
        LOG_WARN("Skipped %zu bytes from the M4 that weren't a message", skipped);
    }

    for (const auto& bytes : messages) {
        protocol::DecodedMessage message;
        if (protocol::decodeMessage(bytes.data(), bytes.size(), message) == protocol::DecodeResult::Ok) {
            handleMessage(message);
        }
    }
}

void RpmsgClient::handleMessage(const protocol::DecodedMessage& message) {
    using protocol::MsgType;

    switch (static_cast<MsgType>(message.header.type)) {
        case MsgType::RSP_PONG:
            LOG_DEBUG("RPMsg: Pong received from M4");
            m_m4Responded = true;
            break;

        case MsgType::RSP_SENSOR_DATA:
        case MsgType::EVT_SENSOR_UPDATE:
            if (message.header.len >= sizeof(protocol::SensorDataPayload)) {
                protocol::SensorDataPayload sensor;
                std::memcpy(&sensor, message.payload, sizeof(sensor));
                if (sensor.scale == 0) {
                    LOG_WARN("RPMsg: Ignoring a reading from sensor %d with no scale", sensor.sensorId);
                    break;
                }
                SensorDataEvent event;
                event.sensorId = std::to_string(sensor.sensorId);
                event.value = static_cast<double>(sensor.value) / sensor.scale;
                switch (static_cast<protocol::SensorType>(sensor.sensorType)) {
                    case protocol::SensorType::Temperature:
                        event.sensorType = "temperature";
                        event.unit = "C";
                        break;
                    case protocol::SensorType::Humidity:
                        event.sensorType = "humidity";
                        event.unit = "%";
                        break;
                    case protocol::SensorType::Pressure: event.sensorType = "pressure"; break;
                    case protocol::SensorType::Light: event.sensorType = "light"; break;
                    case protocol::SensorType::Motion: event.sensorType = "motion"; break;
                    case protocol::SensorType::Proximity: event.sensorType = "proximity"; break;
                    case protocol::SensorType::CO2: event.sensorType = "co2"; break;
                    case protocol::SensorType::VOC: event.sensorType = "voc"; break;
                    default: event.sensorType = "unknown"; break;
                }
                m_eventBus.publish(event);
            }
            break;

        case MsgType::RSP_GPIO_STATE:
        case MsgType::EVT_GPIO_CHANGE:
            LOG_DEBUG("RPMsg: GPIO state received");
            break;

        case MsgType::EVT_ERROR:
            LOG_ERROR("RPMsg: Error received from M4");
            break;

        default:
            LOG_DEBUG("RPMsg: Unknown message type 0x%02X", message.header.type);
            break;
    }

    // The whole message, header included
    const std::vector<uint8_t> raw(message.payload - sizeof(protocol::MsgHeader),
                                   message.payload + message.header.len);

    // Publish raw message event
    RpmsgMessageEvent event;
    event.data = raw;
    m_eventBus.publish(event);

    // Call user callback
    if (m_callback) {
        m_callback(raw);
    }
}

} // namespace smarthub
