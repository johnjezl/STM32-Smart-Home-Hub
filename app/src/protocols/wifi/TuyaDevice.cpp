/**
 * SmartHub Tuya Local Protocol Implementation
 */

#include <smarthub/protocols/wifi/TuyaDevice.hpp>
#include <smarthub/core/Logger.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <cstring>
#include <algorithm>
#include <random>
#include <iomanip>
#include <sstream>

#if __has_include(<openssl/evp.h>)
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#define HAVE_OPENSSL 1
#else
#define HAVE_OPENSSL 0
#endif

namespace smarthub::wifi {

namespace {

// Convert hex string to bytes
std::vector<uint8_t> hexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteStr = hex.substr(i, 2);
        bytes.push_back(static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16)));
    }
    return bytes;
}

// Convert bytes to hex string
std::string bytesToHex(const std::vector<uint8_t>& bytes) {
    std::ostringstream oss;
    for (uint8_t b : bytes) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return oss.str();
}

// Read big-endian uint32
uint32_t readBE32(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

// Write big-endian uint32
void writeBE32(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
}

} // anonymous namespace

// ============= TuyaCrypto =============

TuyaCrypto::TuyaCrypto(const std::string& localKey, const std::string& version)
    : m_version(version) {
    setLocalKey(localKey);

    // Generate local nonce for session negotiation
    m_localNonce.resize(16);
#if HAVE_OPENSSL
    RAND_bytes(m_localNonce.data(), 16);
#else
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    for (auto& b : m_localNonce) {
        b = static_cast<uint8_t>(dis(gen));
    }
#endif
}

void TuyaCrypto::setLocalKey(const std::string& localKey) {
    if (localKey.size() == 32) {
        // Hex-encoded key
        m_localKey = hexToBytes(localKey);
    } else if (localKey.size() == 16) {
        // Raw key
        m_localKey = std::vector<uint8_t>(localKey.begin(), localKey.end());
    } else {
        LOG_WARN("Invalid Tuya local key length: %zu", localKey.size());
        m_localKey = std::vector<uint8_t>(16, 0);
    }
}

void TuyaCrypto::setSessionKey(const std::vector<uint8_t>& key) {
    m_sessionKey = key;
    m_sessionEstablished = true;
}

bool TuyaCrypto::needsSessionNegotiation() const {
    return (m_version == "3.4" || m_version == "3.5") && !m_sessionEstablished;
}

std::vector<uint8_t> TuyaCrypto::getLocalNonce() const {
    return m_localNonce;
}

bool TuyaCrypto::completeSessionNegotiation(const std::vector<uint8_t>& remoteNonce) {
    // Derive session key from local and remote nonces using HMAC-SHA256
    // Simplified: XOR the nonces and encrypt with local key
    if (remoteNonce.size() < 16) {
        return false;
    }

    std::vector<uint8_t> combined(16);
    for (size_t i = 0; i < 16; i++) {
        combined[i] = m_localNonce[i] ^ remoteNonce[i];
    }

    m_sessionKey = aesEcbEncrypt(combined, m_localKey);
    if (m_sessionKey.size() >= 16) {
        m_sessionKey.resize(16);
        m_sessionEstablished = true;
        return true;
    }
    return false;
}

std::vector<uint8_t> TuyaCrypto::pkcs7Pad(const std::vector<uint8_t>& data, size_t blockSize) {
    size_t padLen = blockSize - (data.size() % blockSize);
    std::vector<uint8_t> padded = data;
    padded.insert(padded.end(), padLen, static_cast<uint8_t>(padLen));
    return padded;
}

std::vector<uint8_t> TuyaCrypto::pkcs7Unpad(const std::vector<uint8_t>& data) {
    if (data.empty()) return data;
    uint8_t padLen = data.back();
    if (padLen > 16 || padLen > data.size()) {
        return data;  // Invalid padding
    }
    return std::vector<uint8_t>(data.begin(), data.end() - padLen);
}

std::vector<uint8_t> TuyaCrypto::aesEcbEncrypt(const std::vector<uint8_t>& data,
                                                const std::vector<uint8_t>& key) const {
#if HAVE_OPENSSL
    auto padded = pkcs7Pad(data, 16);
    std::vector<uint8_t> result(padded.size());

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    EVP_CIPHER_CTX_set_padding(ctx, 0);  // We handle padding ourselves
    EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), nullptr, key.data(), nullptr);

    int outLen = 0;
    EVP_EncryptUpdate(ctx, result.data(), &outLen, padded.data(), static_cast<int>(padded.size()));

    int finalLen = 0;
    EVP_EncryptFinal_ex(ctx, result.data() + outLen, &finalLen);

    EVP_CIPHER_CTX_free(ctx);
    result.resize(outLen + finalLen);
    return result;
#else
    LOG_ERROR("AES encryption not available - OpenSSL not compiled in");
    return {};
#endif
}

std::vector<uint8_t> TuyaCrypto::aesEcbDecrypt(const std::vector<uint8_t>& data,
                                                const std::vector<uint8_t>& key) const {
#if HAVE_OPENSSL
    if (data.empty() || data.size() % 16 != 0) {
        return {};
    }

    std::vector<uint8_t> result(data.size());

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    EVP_CIPHER_CTX_set_padding(ctx, 0);
    EVP_DecryptInit_ex(ctx, EVP_aes_128_ecb(), nullptr, key.data(), nullptr);

    int outLen = 0;
    EVP_DecryptUpdate(ctx, result.data(), &outLen, data.data(), static_cast<int>(data.size()));

    int finalLen = 0;
    EVP_DecryptFinal_ex(ctx, result.data() + outLen, &finalLen);

    EVP_CIPHER_CTX_free(ctx);
    result.resize(outLen + finalLen);
    return pkcs7Unpad(result);
#else
    LOG_ERROR("AES decryption not available - OpenSSL not compiled in");
    return {};
#endif
}

std::vector<uint8_t> TuyaCrypto::encrypt(const std::vector<uint8_t>& data) const {
    const auto& key = m_sessionEstablished ? m_sessionKey : m_localKey;
    return aesEcbEncrypt(data, key);
}

std::vector<uint8_t> TuyaCrypto::decrypt(const std::vector<uint8_t>& data) const {
    const auto& key = m_sessionEstablished ? m_sessionKey : m_localKey;
    return aesEcbDecrypt(data, key);
}

std::vector<uint8_t> TuyaCrypto::encrypt(const std::string& data) const {
    return encrypt(std::vector<uint8_t>(data.begin(), data.end()));
}

std::string TuyaCrypto::decryptToString(const std::vector<uint8_t>& data) const {
    auto decrypted = decrypt(data);
    return std::string(decrypted.begin(), decrypted.end());
}

// ============= TuyaMessage =============

TuyaMessage::TuyaMessage(TuyaCommand cmd, uint32_t seqNo)
    : m_command(cmd)
    , m_seqNo(seqNo) {
}

void TuyaMessage::setPayload(const nlohmann::json& payload) {
    std::string s = payload.dump();
    m_payload = std::vector<uint8_t>(s.begin(), s.end());
}

void TuyaMessage::setPayload(const std::string& payload) {
    m_payload = std::vector<uint8_t>(payload.begin(), payload.end());
}

void TuyaMessage::setRawPayload(const std::vector<uint8_t>& data) {
    m_payload = data;
}

nlohmann::json TuyaMessage::jsonPayload() const {
    try {
        std::string s(m_payload.begin(), m_payload.end());
        return nlohmann::json::parse(s);
    } catch (...) {
        return nlohmann::json{};
    }
}

std::string TuyaMessage::stringPayload() const {
    return std::string(m_payload.begin(), m_payload.end());
}

uint32_t TuyaMessage::calculateCrc(const std::vector<uint8_t>& data) {
    // Simple CRC32 calculation
    uint32_t crc = 0xFFFFFFFF;
    for (uint8_t byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

std::vector<uint8_t> TuyaMessage::encode(TuyaCrypto& crypto, const std::string& version) const {
    std::vector<uint8_t> result;

    // Prefix
    writeBE32(result, PREFIX);

    // Sequence number
    writeBE32(result, m_seqNo);

    // Command
    writeBE32(result, static_cast<uint32_t>(m_command));

    // Encrypt payload if needed
    std::vector<uint8_t> encPayload;
    if (!m_payload.empty()) {
        if (version == "3.1") {
            // v3.1: payload is encrypted then base64-encoded then wrapped with version header
            auto encrypted = crypto.encrypt(m_payload);
            // For simplicity, we'll just encrypt directly
            encPayload = encrypted;
        } else {
            // v3.3+: just encrypt
            encPayload = crypto.encrypt(m_payload);
        }
    }

    // Add version header for v3.3+
    std::vector<uint8_t> fullPayload;
    if (version != "3.1") {
        std::string verStr = version + "\0\0\0\0\0\0\0\0\0\0\0\0";
        fullPayload.insert(fullPayload.end(), verStr.begin(), verStr.begin() + 15);
    }
    fullPayload.insert(fullPayload.end(), encPayload.begin(), encPayload.end());

    // Length (payload + CRC + suffix length)
    writeBE32(result, static_cast<uint32_t>(fullPayload.size() + 8));

    // Payload
    result.insert(result.end(), fullPayload.begin(), fullPayload.end());

    // CRC (of header + payload)
    uint32_t crc = calculateCrc(result);
    writeBE32(result, crc);

    // Suffix
    writeBE32(result, SUFFIX);

    return result;
}

std::pair<size_t, size_t> TuyaMessage::findMessage(const std::vector<uint8_t>& data) {
    // Look for prefix
    for (size_t i = 0; i + 20 <= data.size(); i++) {
        if (readBE32(&data[i]) == PREFIX) {
            // Found potential message start
            if (i + 16 > data.size()) {
                return {i, 0};  // Incomplete
            }

            uint32_t length = readBE32(&data[i + 12]);
            size_t totalLen = 16 + length;

            if (i + totalLen > data.size()) {
                return {i, 0};  // Incomplete
            }

            // Verify suffix
            if (readBE32(&data[i + totalLen - 4]) == SUFFIX) {
                return {i, totalLen};
            }
        }
    }
    return {data.size(), 0};  // No message found
}

std::optional<TuyaMessage> TuyaMessage::decode(const std::vector<uint8_t>& data,
                                                TuyaCrypto& crypto,
                                                const std::string& version) {
    if (data.size() < 20) {
        return std::nullopt;
    }

    // Verify prefix
    if (readBE32(&data[0]) != PREFIX) {
        return std::nullopt;
    }

    TuyaMessage msg;
    msg.m_seqNo = readBE32(&data[4]);
    msg.m_command = static_cast<TuyaCommand>(readBE32(&data[8]));

    uint32_t length = readBE32(&data[12]);
    if (data.size() < 16 + length) {
        return std::nullopt;
    }

    // Verify suffix
    if (readBE32(&data[16 + length - 4]) != SUFFIX) {
        return std::nullopt;
    }

    // Extract payload (between header and CRC+suffix)
    size_t payloadLen = length - 8;  // Subtract CRC + suffix
    if (payloadLen == 0) {
        return msg;
    }

    std::vector<uint8_t> encPayload(data.begin() + 16, data.begin() + 16 + payloadLen);

    // Skip version header for v3.3+
    size_t dataStart = 0;
    if (version != "3.1" && payloadLen > 15) {
        dataStart = 15;
    }

    if (dataStart < encPayload.size()) {
        std::vector<uint8_t> toDecrypt(encPayload.begin() + dataStart, encPayload.end());
        auto decrypted = crypto.decrypt(toDecrypt);
        msg.m_payload = decrypted;
    }

    return msg;
}

// ============= TuyaDevice =============

TuyaDevice::TuyaDevice(const std::string& id, const std::string& name,
                       const TuyaDeviceConfig& config)
    : Device(id, name, DeviceType::Switch)
    , m_config(config)
    , m_crypto(config.localKey, config.version) {

    setProtocol("tuya");
    setAddress(config.ipAddress + ":" + std::to_string(config.port));

    // Infer device type from category
    if (config.category == "dj" || config.category == "light") {
        m_type = DeviceType::ColorLight;
    } else if (config.category == "dd" || config.category == "dimmer") {
        m_type = DeviceType::Dimmer;
    } else if (config.category == "wsdcg" || config.category == "sensor") {
        m_type = DeviceType::TemperatureSensor;
    }
}

TuyaDevice::~TuyaDevice() {
    disconnect();
}

bool TuyaDevice::connect() {
    if (m_connected) {
        return true;
    }

    // Create TCP socket
    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket < 0) {
        LOG_ERROR("Failed to create Tuya socket: %s", strerror(errno));
        return false;
    }

    // Set non-blocking
    int flags = fcntl(m_socket, F_GETFL, 0);
    fcntl(m_socket, F_SETFL, flags | O_NONBLOCK);

    // Connect
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_config.port);
    inet_pton(AF_INET, m_config.ipAddress.c_str(), &addr.sin_addr);

    int ret = ::connect(m_socket, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    if (ret < 0 && errno != EINPROGRESS) {
        LOG_ERROR("Tuya connect to %s failed: %s",
                  m_config.ipAddress.c_str(), strerror(errno));
        close(m_socket);
        m_socket = -1;
        return false;
    }

    // Wait for connection with timeout
    struct pollfd pfd{};
    pfd.fd = m_socket;
    pfd.events = POLLOUT;

    ret = poll(&pfd, 1, 5000);  // 5 second timeout
    if (ret <= 0) {
        LOG_ERROR("Tuya connection to %s timed out", m_config.ipAddress.c_str());
        close(m_socket);
        m_socket = -1;
        return false;
    }

    // Check for socket error
    int err = 0;
    socklen_t errLen = sizeof(err);
    getsockopt(m_socket, SOL_SOCKET, SO_ERROR, &err, &errLen);
    if (err != 0) {
        LOG_ERROR("Tuya socket error: %s", strerror(err));
        close(m_socket);
        m_socket = -1;
        return false;
    }

    LOG_INFO("Connected to Tuya device %s at %s",
             m_config.deviceId.c_str(), m_config.ipAddress.c_str());

    // Perform session negotiation for v3.4/3.5
    if (m_crypto.needsSessionNegotiation()) {
        if (!performSessionNegotiation()) {
            LOG_ERROR("Tuya session negotiation failed");
            close(m_socket);
            m_socket = -1;
            return false;
        }
    }

    // Start receive thread
    m_running = true;
    m_connected = true;
    m_thread = std::thread(&TuyaDevice::connectionThread, this);

    setAvailability(DeviceAvailability::Online);

    if (m_connectionCallback) {
        m_connectionCallback(m_config.deviceId, true);
    }

    // Query initial status
    queryStatus();

    return true;
}

void TuyaDevice::disconnect() {
    m_running = false;
    m_connected = false;

    if (m_socket >= 0) {
        close(m_socket);
        m_socket = -1;
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }

    setAvailability(DeviceAvailability::Offline);

    if (m_connectionCallback) {
        m_connectionCallback(m_config.deviceId, false);
    }
}

bool TuyaDevice::performSessionNegotiation() {
    // Send session key negotiation start
    TuyaMessage startMsg(TuyaCommand::SESS_KEY_NEG_START, ++m_seqNo);
    startMsg.setRawPayload(m_crypto.getLocalNonce());

    if (!sendMessage(startMsg)) {
        return false;
    }

    // Wait for response
    std::vector<uint8_t> buffer(1024);
    struct pollfd pfd{};
    pfd.fd = m_socket;
    pfd.events = POLLIN;

    if (poll(&pfd, 1, 5000) <= 0) {
        return false;
    }

    ssize_t n = recv(m_socket, buffer.data(), buffer.size(), 0);
    if (n <= 0) {
        return false;
    }

    buffer.resize(n);
    auto response = TuyaMessage::decode(buffer, m_crypto, m_config.version);
    if (!response || response->command() != TuyaCommand::SESS_KEY_NEG_RESP) {
        return false;
    }

    // Complete negotiation with remote nonce
    return m_crypto.completeSessionNegotiation(response->rawPayload());
}

void TuyaDevice::connectionThread() {
    std::vector<uint8_t> buffer;
    buffer.reserve(4096);

    while (m_running) {
        struct pollfd pfd{};
        pfd.fd = m_socket;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, 1000);
        if (ret < 0) {
            if (errno != EINTR) {
                LOG_ERROR("Tuya poll error: %s", strerror(errno));
                break;
            }
            continue;
        }

        if (ret == 0) {
            // Timeout - send heartbeat
            TuyaMessage hb(TuyaCommand::HEART_BEAT, ++m_seqNo);
            sendMessage(hb);
            continue;
        }

        if (pfd.revents & POLLIN) {
            uint8_t temp[1024];
            ssize_t n = recv(m_socket, temp, sizeof(temp), 0);
            if (n <= 0) {
                if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    continue;
                }
                LOG_WARN("Tuya device %s disconnected", m_config.deviceId.c_str());
                break;
            }

            buffer.insert(buffer.end(), temp, temp + n);

            // Process complete messages
            while (true) {
                auto [start, len] = TuyaMessage::findMessage(buffer);
                if (len == 0) {
                    if (start > 0) {
                        buffer.erase(buffer.begin(), buffer.begin() + start);
                    }
                    break;
                }

                std::vector<uint8_t> msgData(buffer.begin() + start,
                                              buffer.begin() + start + len);
                buffer.erase(buffer.begin(), buffer.begin() + start + len);

                auto msg = TuyaMessage::decode(msgData, m_crypto, m_config.version);
                if (msg) {
                    processMessage(*msg);
                }
            }
        }

        if (pfd.revents & (POLLERR | POLLHUP)) {
            LOG_WARN("Tuya device %s connection error", m_config.deviceId.c_str());
            break;
        }
    }

    m_connected = false;
    setAvailability(DeviceAvailability::Offline);
    if (m_connectionCallback) {
        m_connectionCallback(m_config.deviceId, false);
    }
}

bool TuyaDevice::sendMessage(const TuyaMessage& msg) {
    std::lock_guard<std::mutex> lock(m_sendMutex);

    if (m_socket < 0) {
        return false;
    }

    auto data = msg.encode(m_crypto, m_config.version);

    ssize_t sent = 0;
    while (sent < static_cast<ssize_t>(data.size())) {
        ssize_t n = send(m_socket, data.data() + sent, data.size() - sent, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            LOG_ERROR("Tuya send failed: %s", strerror(errno));
            return false;
        }
        sent += n;
    }

    return true;
}

void TuyaDevice::processMessage(const TuyaMessage& msg) {
    switch (msg.command()) {
        case TuyaCommand::HEART_BEAT:
            // Heartbeat ACK - device is alive
            break;

        case TuyaCommand::STATUS:
        case TuyaCommand::DP_QUERY:
        case TuyaCommand::DP_QUERY_NEW: {
            // Parse data points
            auto json = msg.jsonPayload();
            if (!json.contains("dps")) {
                break;
            }

            std::map<uint8_t, TuyaDataPoint> dps;
            for (auto& [key, val] : json["dps"].items()) {
                TuyaDataPoint dp;
                dp.id = static_cast<uint8_t>(std::stoi(key));
                dp.value = val;

                if (val.is_boolean()) {
                    dp.type = TuyaDataPoint::Type::Bool;
                } else if (val.is_number_integer()) {
                    dp.type = TuyaDataPoint::Type::Int;
                } else if (val.is_string()) {
                    dp.type = TuyaDataPoint::Type::String;
                } else {
                    dp.type = TuyaDataPoint::Type::Raw;
                }

                dps[dp.id] = dp;
            }

            // Update internal state
            {
                std::lock_guard<std::mutex> lock(m_dpMutex);
                for (const auto& [id, dp] : dps) {
                    m_dataPoints[id] = dp;
                }
            }

            // Map common DPs to device state
            if (dps.count(1)) {
                Device::setState("on", dps[1].value);
            }
            if (dps.count(2)) {
                // Brightness (0-1000 typically)
                int bri = dps[2].value.get<int>();
                Device::setState("brightness", bri * 100 / 1000);
            }
            if (dps.count(3)) {
                Device::setState("colorTemp", dps[3].value);
            }

            if (m_stateCallback) {
                m_stateCallback(m_config.deviceId, dps);
            }
            break;
        }

        default:
            LOG_DEBUG("Tuya unknown command: 0x%02X", static_cast<int>(msg.command()));
            break;
    }
}

bool TuyaDevice::setState(const std::string& property, const nlohmann::json& value) {
    // Map property to DP
    if (property == "on") {
        return setDataPoint(1, value);
    } else if (property == "brightness") {
        // Convert 0-100 to 0-1000
        int bri = value.get<int>() * 10;
        return setDataPoint(2, bri);
    } else if (property == "colorTemp") {
        return setDataPoint(3, value);
    }

    return false;
}

bool TuyaDevice::setDataPoint(uint8_t dpId, const nlohmann::json& value) {
    return setDataPoints({{dpId, value}});
}

bool TuyaDevice::setDataPoints(const std::map<uint8_t, nlohmann::json>& dps) {
    if (!m_connected) {
        return false;
    }

    nlohmann::json payload;
    payload["devId"] = m_config.deviceId;
    payload["uid"] = "";
    payload["t"] = std::to_string(std::time(nullptr));

    nlohmann::json dpsJson;
    for (const auto& [id, val] : dps) {
        dpsJson[std::to_string(id)] = val;
    }
    payload["dps"] = dpsJson;

    TuyaCommand cmd = (m_config.version == "3.4" || m_config.version == "3.5")
                      ? TuyaCommand::CONTROL_NEW
                      : TuyaCommand::CONTROL;

    TuyaMessage msg(cmd, ++m_seqNo);
    msg.setPayload(payload);

    return sendMessage(msg);
}

bool TuyaDevice::queryStatus() {
    if (!m_connected) {
        return false;
    }

    nlohmann::json payload;
    payload["devId"] = m_config.deviceId;
    payload["uid"] = "";
    payload["t"] = std::to_string(std::time(nullptr));
    payload["dps"] = nlohmann::json::object();

    TuyaCommand cmd = (m_config.version == "3.4" || m_config.version == "3.5")
                      ? TuyaCommand::DP_QUERY_NEW
                      : TuyaCommand::DP_QUERY;

    TuyaMessage msg(cmd, ++m_seqNo);
    msg.setPayload(payload);

    return sendMessage(msg);
}

// ============= TuyaDiscovery =============

TuyaDiscovery::~TuyaDiscovery() {
    stop();
}

bool TuyaDiscovery::start() {
    if (m_running) {
        return true;
    }

    m_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket < 0) {
        LOG_ERROR("Failed to create Tuya discovery socket");
        return false;
    }

    // Allow address reuse
    int opt = 1;
    setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind to broadcast port
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(6666);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(m_socket, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        LOG_ERROR("Failed to bind Tuya discovery socket: %s", strerror(errno));
        close(m_socket);
        m_socket = -1;
        return false;
    }

    m_running = true;
    m_thread = std::thread(&TuyaDiscovery::listenThread, this);

    LOG_INFO("Tuya UDP discovery started");
    return true;
}

void TuyaDiscovery::stop() {
    m_running = false;

    if (m_socket >= 0) {
        close(m_socket);
        m_socket = -1;
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void TuyaDiscovery::listenThread() {
    std::vector<uint8_t> buffer(2048);

    while (m_running) {
        struct pollfd pfd{};
        pfd.fd = m_socket;
        pfd.events = POLLIN;

        if (poll(&pfd, 1, 1000) <= 0) {
            continue;
        }

        struct sockaddr_in srcAddr{};
        socklen_t srcLen = sizeof(srcAddr);

        ssize_t n = recvfrom(m_socket, buffer.data(), buffer.size(), 0,
                             reinterpret_cast<struct sockaddr*>(&srcAddr), &srcLen);
        if (n <= 0) {
            continue;
        }

        // Tuya broadcast messages are encrypted with a known key
        // Format: prefix + encrypted JSON with device info
        // For now, we'll just log that we received something
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &srcAddr.sin_addr, ipStr, sizeof(ipStr));

        LOG_DEBUG("Tuya broadcast from %s, %zd bytes", ipStr, n);

        // Parse and notify if valid
        // Full parsing would require the broadcast decryption key
        // which is device-specific or uses a default key
    }
}

} // namespace smarthub::wifi
