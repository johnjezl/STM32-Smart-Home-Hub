/**
 * ZNP Transport Implementation
 */

#include <smarthub/protocols/zigbee/ZnpTransport.hpp>
#include <smarthub/core/Logger.hpp>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <cstring>
#include <chrono>

namespace smarthub::zigbee {

// ============================================================================
// PosixSerialPort Implementation
// ============================================================================

PosixSerialPort::~PosixSerialPort() {
    close();
}

bool PosixSerialPort::open(const std::string& port, int baudRate) {
    m_fd = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (m_fd < 0) {
        LOG_ERROR("Failed to open serial port {}: {}", port, strerror(errno));
        return false;
    }

    // Configure serial port
    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(m_fd, &tty) != 0) {
        LOG_ERROR("Failed to get serial attributes: {}", strerror(errno));
        close();
        return false;
    }

    // Set baud rate
    speed_t speed;
    switch (baudRate) {
        case 9600:   speed = B9600;   break;
        case 19200:  speed = B19200;  break;
        case 38400:  speed = B38400;  break;
        case 57600:  speed = B57600;  break;
        case 115200: speed = B115200; break;
        case 230400: speed = B230400; break;
        case 460800: speed = B460800; break;
        default:
            LOG_ERROR("Unsupported baud rate: {}", baudRate);
            close();
            return false;
    }

    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    // 8N1, no flow control
    tty.c_cflag &= ~PARENB;         // No parity
    tty.c_cflag &= ~CSTOPB;         // 1 stop bit
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;             // 8 data bits
    tty.c_cflag &= ~CRTSCTS;        // No hardware flow control
    tty.c_cflag |= CREAD | CLOCAL;  // Enable receiver, ignore modem status

    // Raw input
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);  // No software flow control
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    // Raw output
    tty.c_oflag &= ~OPOST;
    tty.c_oflag &= ~ONLCR;

    // Non-blocking read with timeout
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;  // 100ms timeout

    if (tcsetattr(m_fd, TCSANOW, &tty) != 0) {
        LOG_ERROR("Failed to set serial attributes: {}", strerror(errno));
        close();
        return false;
    }

    // Flush buffers
    tcflush(m_fd, TCIOFLUSH);

    LOG_INFO("Opened serial port {} at {} baud", port, baudRate);
    return true;
}

void PosixSerialPort::close() {
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

ssize_t PosixSerialPort::write(const uint8_t* data, size_t len) {
    if (m_fd < 0) return -1;
    return ::write(m_fd, data, len);
}

ssize_t PosixSerialPort::read(uint8_t* buffer, size_t maxLen, int timeoutMs) {
    if (m_fd < 0) return -1;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(m_fd, &readfds);

    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int ret = select(m_fd + 1, &readfds, nullptr, nullptr, &tv);
    if (ret > 0 && FD_ISSET(m_fd, &readfds)) {
        return ::read(m_fd, buffer, maxLen);
    }

    return 0;  // Timeout
}

bool PosixSerialPort::setDTR(bool state) {
    if (m_fd < 0) return false;

    int flags;
    if (ioctl(m_fd, TIOCMGET, &flags) < 0) return false;

    if (state) {
        flags |= TIOCM_DTR;
    } else {
        flags &= ~TIOCM_DTR;
    }

    return ioctl(m_fd, TIOCMSET, &flags) >= 0;
}

bool PosixSerialPort::setRTS(bool state) {
    if (m_fd < 0) return false;

    int flags;
    if (ioctl(m_fd, TIOCMGET, &flags) < 0) return false;

    if (state) {
        flags |= TIOCM_RTS;
    } else {
        flags &= ~TIOCM_RTS;
    }

    return ioctl(m_fd, TIOCMSET, &flags) >= 0;
}

// ============================================================================
// ZnpTransport Implementation
// ============================================================================

ZnpTransport::ZnpTransport(const std::string& port, int baudRate)
    : m_serialPort(std::make_unique<PosixSerialPort>())
    , m_port(port)
    , m_baudRate(baudRate)
{
}

ZnpTransport::ZnpTransport(std::unique_ptr<ISerialPort> serialPort,
                           const std::string& port, int baudRate)
    : m_serialPort(std::move(serialPort))
    , m_port(port)
    , m_baudRate(baudRate)
{
}

ZnpTransport::~ZnpTransport() {
    close();
}

bool ZnpTransport::open() {
    if (!m_serialPort->open(m_port, m_baudRate)) {
        return false;
    }

    // Start reader thread
    m_running = true;
    m_readerThread = std::thread(&ZnpTransport::readerThread, this);

    LOG_INFO("ZNP transport opened on {}", m_port);
    return true;
}

void ZnpTransport::close() {
    if (m_running) {
        m_running = false;

        // Wake up any waiting request
        {
            std::lock_guard<std::mutex> lock(m_requestMutex);
            m_responseCv.notify_all();
        }

        if (m_readerThread.joinable()) {
            m_readerThread.join();
        }
    }

    m_serialPort->close();
    m_rxBuffer.clear();

    LOG_INFO("ZNP transport closed");
}

bool ZnpTransport::send(const ZnpFrame& frame) {
    if (!isOpen()) {
        LOG_ERROR("Cannot send: transport not open");
        return false;
    }

    auto data = frame.serialize();

    LOG_DEBUG("TX: {}", frame.toString());

    ssize_t written = m_serialPort->write(data.data(), data.size());
    if (written != static_cast<ssize_t>(data.size())) {
        LOG_ERROR("Failed to write frame: wrote {} of {} bytes",
                  written, data.size());
        return false;
    }

    return true;
}

std::optional<ZnpFrame> ZnpTransport::request(const ZnpFrame& frame, int timeoutMs) {
    if (!frame.isRequest()) {
        LOG_ERROR("request() called with non-SREQ frame");
        return std::nullopt;
    }

    std::unique_lock<std::mutex> lock(m_requestMutex);

    // Set up expected response
    // Response has same subsystem and command, but SRSP type
    m_expectedCmd0 = static_cast<uint8_t>(ZnpType::SRSP) |
                     static_cast<uint8_t>(frame.subsystem());
    m_expectedCmd1 = frame.command();
    m_pendingResponse.reset();
    m_waitingForResponse = true;

    // Send the request
    if (!send(frame)) {
        m_waitingForResponse = false;
        return std::nullopt;
    }

    // Wait for response
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeoutMs);

    while (m_running && !m_pendingResponse) {
        if (m_responseCv.wait_until(lock, deadline) == std::cv_status::timeout) {
            LOG_WARN("Timeout waiting for response to {}", frame.toString());
            m_waitingForResponse = false;
            return std::nullopt;
        }
    }

    m_waitingForResponse = false;

    if (m_pendingResponse) {
        LOG_DEBUG("RX: {}", m_pendingResponse->toString());
        return std::move(m_pendingResponse);
    }

    return std::nullopt;
}

void ZnpTransport::setIndicationCallback(IndicationCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_indicationCallback = std::move(callback);
}

bool ZnpTransport::resetCoordinator() {
    LOG_INFO("Resetting coordinator via DTR/RTS...");

    // Toggle DTR to reset the coordinator
    m_serialPort->setDTR(false);
    m_serialPort->setRTS(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    m_serialPort->setDTR(true);
    m_serialPort->setRTS(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    return true;
}

void ZnpTransport::readerThread() {
    uint8_t buffer[256];

    while (m_running) {
        ssize_t bytesRead = m_serialPort->read(buffer, sizeof(buffer), 100);

        if (bytesRead > 0) {
            std::lock_guard<std::mutex> lock(m_rxMutex);
            m_rxBuffer.insert(m_rxBuffer.end(), buffer, buffer + bytesRead);
            processReceivedData();
        }
    }
}

void ZnpTransport::processReceivedData() {
    // Try to find and parse complete frames in the buffer
    while (m_rxBuffer.size() >= ZnpFrame::MIN_FRAME_SIZE) {
        size_t frameStart, frameLen;

        if (ZnpFrame::findFrame(m_rxBuffer.data(), m_rxBuffer.size(),
                                frameStart, frameLen)) {
            // Remove any garbage before the frame
            if (frameStart > 0) {
                m_rxBuffer.erase(m_rxBuffer.begin(),
                                 m_rxBuffer.begin() + frameStart);
            }

            // Parse the frame
            auto frame = ZnpFrame::parse(m_rxBuffer.data(), m_rxBuffer.size());
            if (frame) {
                // Remove the parsed frame from buffer
                m_rxBuffer.erase(m_rxBuffer.begin(),
                                 m_rxBuffer.begin() + frameLen);

                // Dispatch the frame
                dispatchFrame(*frame);
            } else {
                // Parse failed, remove SOF and try again
                m_rxBuffer.erase(m_rxBuffer.begin());
            }
        } else {
            // No complete frame yet
            break;
        }
    }

    // Prevent buffer from growing too large
    if (m_rxBuffer.size() > 4096) {
        LOG_WARN("Discarding {} bytes of garbage data", m_rxBuffer.size());
        m_rxBuffer.clear();
    }
}

void ZnpTransport::dispatchFrame(const ZnpFrame& frame) {
    if (frame.isResponse()) {
        // Check if this is the response we're waiting for
        std::lock_guard<std::mutex> lock(m_requestMutex);

        if (m_waitingForResponse &&
            frame.cmd0() == m_expectedCmd0 &&
            frame.cmd1() == m_expectedCmd1) {
            m_pendingResponse = frame;
            m_responseCv.notify_one();
        } else {
            LOG_WARN("Unexpected response: {}", frame.toString());
        }
    } else if (frame.isIndication()) {
        // Dispatch to indication callback
        std::lock_guard<std::mutex> lock(m_callbackMutex);

        if (m_indicationCallback) {
            LOG_DEBUG("Indication: {}", frame.toString());
            m_indicationCallback(frame);
        }
    }
}

} // namespace smarthub::zigbee
