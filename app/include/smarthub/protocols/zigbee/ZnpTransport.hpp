/**
 * ZNP Serial Transport Layer
 *
 * Handles serial communication with the Zigbee coordinator (CC2652P).
 * Provides synchronous request/response and async indication handling.
 */
#pragma once

#include "ZnpFrame.hpp"
#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <memory>

namespace smarthub::zigbee {

/**
 * Callback for received indications (AREQ frames)
 */
using IndicationCallback = std::function<void(const ZnpFrame&)>;

/**
 * Serial port interface for dependency injection (allows mocking)
 */
class ISerialPort {
public:
    virtual ~ISerialPort() = default;

    virtual bool open(const std::string& port, int baudRate) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    virtual ssize_t write(const uint8_t* data, size_t len) = 0;
    virtual ssize_t read(uint8_t* buffer, size_t maxLen, int timeoutMs) = 0;

    virtual bool setDTR(bool state) = 0;
    virtual bool setRTS(bool state) = 0;
};

/**
 * Real serial port implementation using POSIX APIs
 */
class PosixSerialPort : public ISerialPort {
public:
    PosixSerialPort() = default;
    ~PosixSerialPort() override;

    bool open(const std::string& port, int baudRate) override;
    void close() override;
    bool isOpen() const override { return m_fd >= 0; }

    ssize_t write(const uint8_t* data, size_t len) override;
    ssize_t read(uint8_t* buffer, size_t maxLen, int timeoutMs) override;

    bool setDTR(bool state) override;
    bool setRTS(bool state) override;

private:
    int m_fd = -1;
};

/**
 * ZNP Transport layer for serial communication with Zigbee coordinator
 */
class ZnpTransport {
public:
    /**
     * Construct transport with port and baud rate
     * Uses real serial port by default
     */
    explicit ZnpTransport(const std::string& port, int baudRate = 115200);

    /**
     * Construct transport with custom serial port (for testing)
     */
    ZnpTransport(std::unique_ptr<ISerialPort> serialPort,
                 const std::string& port, int baudRate = 115200);

    ~ZnpTransport();

    // Non-copyable
    ZnpTransport(const ZnpTransport&) = delete;
    ZnpTransport& operator=(const ZnpTransport&) = delete;

    /**
     * Open the serial port and start the reader thread
     */
    bool open();

    /**
     * Close the serial port and stop the reader thread
     */
    void close();

    /**
     * Check if transport is open
     */
    bool isOpen() const { return m_serialPort && m_serialPort->isOpen(); }

    /**
     * Send a request and wait for the synchronous response
     * @param frame Request frame (SREQ)
     * @param timeoutMs Response timeout in milliseconds
     * @return Response frame (SRSP) or nullopt on timeout/error
     */
    std::optional<ZnpFrame> request(const ZnpFrame& frame, int timeoutMs = 5000);

    /**
     * Send a frame without waiting for response
     * Used for async operations
     */
    bool send(const ZnpFrame& frame);

    /**
     * Set callback for async indications (AREQ frames)
     */
    void setIndicationCallback(IndicationCallback callback);

    /**
     * Reset the coordinator using DTR/RTS pins
     */
    bool resetCoordinator();

    /**
     * Get port name
     */
    const std::string& portName() const { return m_port; }

    /**
     * Get baud rate
     */
    int baudRate() const { return m_baudRate; }

private:
    void readerThread();
    void processReceivedData();
    void dispatchFrame(const ZnpFrame& frame);

    std::unique_ptr<ISerialPort> m_serialPort;
    std::string m_port;
    int m_baudRate;

    std::thread m_readerThread;
    std::atomic<bool> m_running{false};

    // Receive buffer
    std::vector<uint8_t> m_rxBuffer;
    std::mutex m_rxMutex;

    // Synchronous request/response handling
    std::mutex m_requestMutex;
    std::condition_variable m_responseCv;
    std::optional<ZnpFrame> m_pendingResponse;
    uint8_t m_expectedCmd0 = 0;
    uint8_t m_expectedCmd1 = 0;
    bool m_waitingForResponse = false;

    // Indication callback
    IndicationCallback m_indicationCallback;
    std::mutex m_callbackMutex;
};

} // namespace smarthub::zigbee
