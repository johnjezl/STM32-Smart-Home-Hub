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
#include <cstring>

namespace smarthub {

RpmsgClient::RpmsgClient(EventBus& eventBus, const std::string& device)
    : m_eventBus(eventBus)
    , m_device(device)
{
}

RpmsgClient::~RpmsgClient() {
    shutdown();
}

bool RpmsgClient::initialize() {
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

    // Send initial ping to verify M4 is responsive
    return ping();
}

void RpmsgClient::shutdown() {
    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }
    m_connected = false;
}

void RpmsgClient::poll() {
    if (!m_connected || m_fd < 0) {
        return;
    }

    // Check for available data
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(m_fd, &readfds);

    struct timeval tv = {0, 0}; // Non-blocking
    int ret = select(m_fd + 1, &readfds, nullptr, nullptr, &tv);

    if (ret > 0 && FD_ISSET(m_fd, &readfds)) {
        uint8_t buffer[256];
        ssize_t n = read(m_fd, buffer, sizeof(buffer));

        if (n > 0) {
            std::vector<uint8_t> data(buffer, buffer + n);
            handleMessage(data);
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

bool RpmsgClient::sendMessage(RpmsgMessageType type, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> msg;
    msg.push_back(static_cast<uint8_t>(type));
    msg.push_back(static_cast<uint8_t>(payload.size()));
    msg.insert(msg.end(), payload.begin(), payload.end());

    return send(msg);
}

bool RpmsgClient::requestSensorData(uint8_t sensorId) {
    return sendMessage(RpmsgMessageType::SensorData, {sensorId});
}

bool RpmsgClient::setGpio(uint8_t pin, bool state) {
    return sendMessage(RpmsgMessageType::GpioCommand, {pin, static_cast<uint8_t>(state ? 1 : 0)});
}

bool RpmsgClient::setPwm(uint8_t channel, uint16_t dutyCycle) {
    return sendMessage(RpmsgMessageType::PwmCommand, {
        channel,
        static_cast<uint8_t>(dutyCycle & 0xFF),
        static_cast<uint8_t>((dutyCycle >> 8) & 0xFF)
    });
}

bool RpmsgClient::ping() {
    return sendMessage(RpmsgMessageType::Ping, {});
}

void RpmsgClient::handleMessage(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return;
    }

    auto type = static_cast<RpmsgMessageType>(data[0]);

    switch (type) {
        case RpmsgMessageType::Pong:
            LOG_DEBUG("RPMsg: Pong received from M4");
            break;

        case RpmsgMessageType::SensorData:
            if (data.size() >= 6) {
                // Parse sensor data: [type, len, sensorId, valueL, valueH, valueHH]
                SensorDataEvent event;
                event.sensorId = std::to_string(data[2]);
                // Simplified - real implementation would decode sensor type and value
                event.value = static_cast<double>(data[3] | (data[4] << 8));
                m_eventBus.publish(event);
            }
            break;

        case RpmsgMessageType::GpioState:
            LOG_DEBUG("RPMsg: GPIO state received");
            break;

        case RpmsgMessageType::Error:
            LOG_ERROR("RPMsg: Error received from M4");
            break;

        default:
            LOG_DEBUG("RPMsg: Unknown message type 0x%02X", static_cast<int>(type));
            break;
    }

    // Publish raw message event
    RpmsgMessageEvent event;
    event.data = data;
    m_eventBus.publish(event);

    // Call user callback
    if (m_callback) {
        m_callback(data);
    }
}

} // namespace smarthub
