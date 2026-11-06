/**
 * Fake CC2652P coordinator for tests that need no hardware.
 *
 * Sits behind ISerialPort, so the real ZnpTransport, ZigbeeCoordinator and
 * ZigbeeHandler run unchanged. It decodes the frames they write, answers each
 * SREQ with an SRSP, plays scripted devices, and records everything sent so
 * tests can assert on the frames that would have reached the radio.
 *
 * Must outlive any serial port it creates.
 */
#pragma once

#include <smarthub/protocols/zigbee/ZnpTransport.hpp>
#include <smarthub/protocols/zigbee/ZclConstants.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <utility>
#include <vector>

namespace smarthub::zigbee {

/** A device on the fake network, answering ZDO discovery requests. */
struct FakeZigbeeDevice {
    uint16_t nwkAddr = 0;
    uint64_t ieeeAddr = 0;
    /** Endpoint -> input clusters */
    std::map<uint8_t, std::vector<uint16_t>> inClusters;
    std::string manufacturer = {};
    std::string model = {};
    std::optional<uint16_t> zoneType = {};
    std::optional<uint16_t> zoneStatus = {};
    /** Doesn't answer Read Attributes at all, rather than saying it has no such attribute */
    bool ignoresReads = false;
};

class FakeZnpCoordinator {
public:
    /** Returns the SRSP payload, or nullopt to send no SRSP at all. May call this fake. */
    using Responder = std::function<std::optional<std::vector<uint8_t>>(const ZnpFrame&)>;

    static constexpr uint64_t COORDINATOR_IEEE = 0x00124B0000000001ULL;

    std::unique_ptr<ISerialPort> createSerialPort() {
        return std::make_unique<Port>(*this);
    }

    // ---- Scripting ---------------------------------------------------------

    void addDevice(const FakeZigbeeDevice& device) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_devices[device.nwkAddr] = device;
    }

    /** Override the SRSP for one command. */
    void setResponder(ZnpSubsystem subsystem, uint8_t command, Responder responder) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_responders[{subsystem, command}] = std::move(responder);
    }

    /** It has gone from the network, so lookups no longer find it. */
    void removeDevice(uint16_t nwkAddr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_devices.erase(nwkAddr);
    }

    /** Hold back a device's reply about this endpoint until releaseDescriptors(). */
    void holdDescriptor(uint16_t nwkAddr, uint8_t endpoint) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_heldEndpoints.insert({nwkAddr, endpoint});
    }

    /** Make a device refuse to describe this endpoint. */
    void failDescriptor(uint16_t nwkAddr, uint8_t endpoint) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_failedEndpoints.insert({nwkAddr, endpoint});
    }

    void releaseDescriptors() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_heldEndpoints.clear();
        for (const auto& held : m_heldFrames) {
            queueFrame(held.second);
        }
        m_heldFrames.clear();
        m_rxCv.notify_all();
    }

    /** Release one held reply for this endpoint, and keep holding any others. */
    void releaseNextDescriptor(uint16_t nwkAddr, uint8_t endpoint) {
        std::lock_guard<std::mutex> lock(m_mutex);
        const std::pair<uint16_t, uint8_t> key{nwkAddr, endpoint};
        auto next = std::find_if(m_heldFrames.begin(), m_heldFrames.end(),
                                 [&](const auto& held) { return held.first == key; });
        if (next != m_heldFrames.end()) {
            queueFrame(next->second);
            m_heldFrames.erase(next);
        }
        m_rxCv.notify_all();
    }

    /** Release all of one endpoint's held replies. */
    void releaseDescriptor(uint16_t nwkAddr, uint8_t endpoint) {
        std::lock_guard<std::mutex> lock(m_mutex);
        const std::pair<uint16_t, uint8_t> key{nwkAddr, endpoint};
        m_heldEndpoints.erase(key);
        for (auto it = m_heldFrames.begin(); it != m_heldFrames.end();) {
            if (it->first == key) {
                queueFrame(it->second);
                it = m_heldFrames.erase(it);
            } else {
                ++it;
            }
        }
        m_rxCv.notify_all();
    }

    /** When set, every SREQ goes unanswered, like a dead or missing radio. */
    void setUnresponsive(bool unresponsive) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_unresponsive = unresponsive;
    }

    /** Limit bytes per read, to exercise frames split across reads. */
    void setMaxReadChunk(size_t bytes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_maxReadChunk = bytes;
    }

    // ---- Things the radio does on its own ----------------------------------

    void injectRaw(const std::vector<uint8_t>& bytes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_rx.insert(m_rx.end(), bytes.begin(), bytes.end());
        m_rxCv.notify_all();
    }

    void injectIndication(const ZnpFrame& frame) {
        injectRaw(frame.serialize());
    }

    /** ZDO_END_DEVICE_ANNCE_IND for a device added with addDevice(). */
    void announceDevice(const FakeZigbeeDevice& device) {
        ZnpFrame frame(ZnpType::AREQ, ZnpSubsystem::ZDO, cmd::zdo::END_DEVICE_ANNCE_IND);
        frame.appendWord(device.nwkAddr);   // SrcAddr
        frame.appendWord(device.nwkAddr);   // NwkAddr
        frame.appendQWord(device.ieeeAddr);
        frame.appendByte(0x8E);             // Capabilities: FFD, mains, rx-on
        injectIndication(frame);
    }

    /** ZDO_LEAVE_IND */
    void deviceLeft(const FakeZigbeeDevice& device) {
        ZnpFrame frame(ZnpType::AREQ, ZnpSubsystem::ZDO, cmd::zdo::LEAVE_IND);
        frame.appendWord(device.nwkAddr);
        frame.appendQWord(device.ieeeAddr);
        frame.appendByte(0);                // Request
        frame.appendByte(0);                // Remove
        frame.appendByte(0);                // Rejoin
        injectIndication(frame);
    }

    /** AF_INCOMING_MSG carrying a ZCL Report Attributes command. */
    void reportAttribute(uint16_t nwkAddr, uint8_t endpoint, uint16_t cluster,
                         uint16_t attributeId, uint8_t dataType,
                         const std::vector<uint8_t>& value) {
        std::vector<uint8_t> zcl = {0x18, nextSeq(), zcl::global_cmd::REPORT_ATTRIBUTES,
                                    static_cast<uint8_t>(attributeId & 0xFF),
                                    static_cast<uint8_t>(attributeId >> 8),
                                    dataType};
        zcl.insert(zcl.end(), value.begin(), value.end());
        injectIndication(incomingMessage(nwkAddr, endpoint, cluster, zcl));
    }

    /** AF_INCOMING_MSG carrying a cluster-specific ZCL command. */
    void clusterCommand(uint16_t nwkAddr, uint8_t endpoint, uint16_t cluster,
                        uint8_t command, const std::vector<uint8_t>& payload) {
        std::vector<uint8_t> zcl = {zcl::frame_ctrl::CLUSTER_SPECIFIC, nextSeq(), command};
        zcl.insert(zcl.end(), payload.begin(), payload.end());
        injectIndication(incomingMessage(nwkAddr, endpoint, cluster, zcl));
    }

    /** AF_INCOMING_MSG with arbitrary ZCL bytes. */
    static ZnpFrame incomingMessage(uint16_t nwkAddr, uint8_t endpoint, uint16_t cluster,
                                    const std::vector<uint8_t>& zcl) {
        ZnpFrame frame(ZnpType::AREQ, ZnpSubsystem::AF, cmd::af::INCOMING_MSG);
        frame.appendWord(0);                // GroupId
        frame.appendWord(cluster);
        frame.appendWord(nwkAddr);
        frame.appendByte(endpoint);         // SrcEndpoint
        frame.appendByte(1);                // DstEndpoint
        frame.appendByte(0);                // WasBroadcast
        frame.appendByte(200);              // LinkQuality
        frame.appendByte(0);                // SecurityUse
        frame.appendDWord(0);               // Timestamp
        frame.appendByte(0);                // TransSeq
        frame.appendByte(static_cast<uint8_t>(zcl.size()));
        frame.appendBytes(zcl);
        return frame;
    }

    // ---- Assertions --------------------------------------------------------

    /** Frames the host has written with this subsystem and command. */
    std::vector<ZnpFrame> sentFrames(ZnpSubsystem subsystem, uint8_t command) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return filtered(subsystem, command);
    }

    bool waitForSent(ZnpSubsystem subsystem, uint8_t command, size_t count = 1,
                     std::chrono::milliseconds timeout = std::chrono::seconds(5)) const {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_sentCv.wait_for(lock, timeout, [&] {
            return filtered(subsystem, command).size() >= count;
        });
    }

    void clearSentFrames() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sent.clear();
    }

private:
    class Port : public ISerialPort {
    public:
        explicit Port(FakeZnpCoordinator& fake) : m_fake(fake) {}

        bool open(const std::string&, int) override { m_open = true; return true; }
        void close() override { m_open = false; }
        bool isOpen() const override { return m_open; }

        ssize_t write(const uint8_t* data, size_t len) override {
            m_fake.onHostWrite(data, len);
            return static_cast<ssize_t>(len);
        }

        ssize_t read(uint8_t* buffer, size_t maxLen, int timeoutMs) override {
            return m_fake.onHostRead(buffer, maxLen, timeoutMs);
        }

        bool setDTR(bool) override { return true; }
        bool setRTS(bool) override { return true; }

    private:
        FakeZnpCoordinator& m_fake;
        std::atomic<bool> m_open{false};
    };

    std::vector<ZnpFrame> filtered(ZnpSubsystem subsystem, uint8_t command) const {
        std::vector<ZnpFrame> out;
        for (const auto& frame : m_sent) {
            if (frame.subsystem() == subsystem && frame.command() == command) {
                out.push_back(frame);
            }
        }
        return out;
    }

    uint8_t nextSeq() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_zclSeq++;
    }

    void onHostWrite(const uint8_t* data, size_t len) {
        std::vector<ZnpFrame> requests;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_tx.insert(m_tx.end(), data, data + len);

            size_t start = 0;
            size_t frameLen = 0;
            while (ZnpFrame::findFrame(m_tx.data(), m_tx.size(), start, frameLen)) {
                auto frame = ZnpFrame::parse(m_tx.data() + start, frameLen);
                m_tx.erase(m_tx.begin(), m_tx.begin() + start + frameLen);
                if (frame) {
                    m_sent.push_back(*frame);
                    if (frame->isRequest() && !m_unresponsive) {
                        requests.push_back(*frame);
                    }
                }
            }
            m_sentCv.notify_all();
        }

        for (const auto& request : requests) {
            respond(request);
        }
    }

    ssize_t onHostRead(uint8_t* buffer, size_t maxLen, int timeoutMs) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_rxCv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                        [&] { return !m_rx.empty(); });

        size_t count = std::min({maxLen, m_rx.size(), m_maxReadChunk});
        std::copy_n(m_rx.begin(), count, buffer);
        m_rx.erase(m_rx.begin(), m_rx.begin() + static_cast<std::ptrdiff_t>(count));
        return static_cast<ssize_t>(count);
    }

    void queueFrame(const ZnpFrame& frame) {
        auto bytes = frame.serialize();
        m_rx.insert(m_rx.end(), bytes.begin(), bytes.end());
    }

    void respond(const ZnpFrame& req) {
        Responder responder;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto found = m_responders.find({req.subsystem(), req.command()});
            if (found != m_responders.end()) responder = found->second;
        }

        // Unlocked, so responders can call back into the fake.
        std::optional<std::vector<uint8_t>> payload =
            responder ? responder(req) : defaultResponse(req);

        std::lock_guard<std::mutex> lock(m_mutex);
        if (payload) {
            queueFrame(ZnpFrame(ZnpType::SRSP, req.subsystem(), req.command(), *payload));
        }
        if (req.subsystem() == ZnpSubsystem::ZDO) {
            answerZdoRequest(req);
        }
        if (req.subsystem() == ZnpSubsystem::AF && req.command() == cmd::af::DATA_REQUEST) {
            answerReadAttributes(req);
        }
        m_rxCv.notify_all();
    }

    // Caller holds m_mutex
    void answerReadAttributes(const ZnpFrame& req) {
        constexpr size_t kZclStart = 10;
        if (req.payload().size() < kZclStart + 3 ||
            req.getByte(kZclStart + 2) != zcl::global_cmd::READ_ATTRIBUTES ||
            (req.getByte(kZclStart) & zcl::frame_ctrl::CLUSTER_SPECIFIC) != 0) {
            return;
        }
        const uint16_t nwkAddr = req.getWord(0);
        const uint8_t endpoint = req.getByte(2);
        const uint16_t cluster = req.getWord(4);
        auto device = m_devices.find(nwkAddr);
        if (device == m_devices.end() || device->second.ignoresReads) {
            return;
        }

        std::vector<uint8_t> zcl = {0x18, req.getByte(kZclStart + 1), zcl::global_cmd::READ_ATTRIBUTES_RSP};
        const auto addString = [&zcl](const std::string& text) {
            zcl.push_back(zcl::datatype::CHAR_STR);
            zcl.push_back(static_cast<uint8_t>(text.size()));
            zcl.insert(zcl.end(), text.begin(), text.end());
        };
        for (size_t offset = kZclStart + 3; offset + 2 <= req.payload().size(); offset += 2) {
            const uint16_t attrId = req.getWord(offset);
            zcl.push_back(static_cast<uint8_t>(attrId & 0xFF));
            zcl.push_back(static_cast<uint8_t>(attrId >> 8));
            if (cluster == zcl::cluster::BASIC && attrId == zcl::attr::basic::MANUFACTURER_NAME &&
                !device->second.manufacturer.empty()) {
                zcl.push_back(zcl::status::SUCCESS);
                addString(device->second.manufacturer);
            } else if (cluster == zcl::cluster::BASIC && attrId == zcl::attr::basic::MODEL_ID &&
                       !device->second.model.empty()) {
                zcl.push_back(zcl::status::SUCCESS);
                addString(device->second.model);
            } else if (cluster == zcl::cluster::IAS_ZONE && attrId == zcl::attr::ias_zone::ZONE_TYPE &&
                       device->second.zoneType) {
                zcl.push_back(zcl::status::SUCCESS);
                zcl.push_back(zcl::datatype::ENUM16);
                zcl.push_back(static_cast<uint8_t>(*device->second.zoneType & 0xFF));
                zcl.push_back(static_cast<uint8_t>(*device->second.zoneType >> 8));
            } else if (cluster == zcl::cluster::IAS_ZONE && attrId == zcl::attr::ias_zone::ZONE_STATUS &&
                       device->second.zoneStatus) {
                zcl.push_back(zcl::status::SUCCESS);
                zcl.push_back(zcl::datatype::BITMAP16);
                zcl.push_back(static_cast<uint8_t>(*device->second.zoneStatus & 0xFF));
                zcl.push_back(static_cast<uint8_t>(*device->second.zoneStatus >> 8));
            } else {
                zcl.push_back(zcl::status::UNSUPPORTED_ATTRIBUTE);
            }
        }
        queueFrame(incomingMessage(nwkAddr, endpoint, cluster, zcl));
    }

    std::vector<uint8_t> defaultResponse(const ZnpFrame& req) const {
        if (req.subsystem() == ZnpSubsystem::SYS && req.command() == cmd::sys::PING) {
            return {0x79, 0x01};    // Capabilities
        }
        if (req.subsystem() == ZnpSubsystem::SYS && req.command() == cmd::sys::VERSION) {
            return {2, 1, 2, 7, 1};
        }
        if (req.subsystem() == ZnpSubsystem::UTIL && req.command() == cmd::util::GET_DEVICE_INFO) {
            ZnpFrame info;
            info.appendByte(0);     // Status
            info.appendQWord(COORDINATOR_IEEE);
            info.appendWord(0x0000);
            info.appendByte(0x07);  // Device type
            info.appendByte(static_cast<uint8_t>(ZnpDeviceState::ZB_COORD));
            info.appendByte(0);     // Associated device count
            return info.payload();
        }
        return {0};                 // Status: SUCCESS
    }

    void queueOrHold(uint16_t nwkAddr, uint8_t endpoint, const ZnpFrame& frame) {
        const std::pair<uint16_t, uint8_t> key{nwkAddr, endpoint};
        if (m_heldEndpoints.count(key) > 0) {
            m_heldFrames.emplace_back(key, frame);
        } else {
            queueFrame(frame);
        }
    }

    // Requests that a real device answers later with an AREQ.
    void answerZdoRequest(const ZnpFrame& req) {
        if (req.command() == cmd::zdo::IEEE_ADDR_REQ) {
            auto device = m_devices.find(req.getWord(0));
            if (device == m_devices.end()) return;

            ZnpFrame rsp(ZnpType::AREQ, ZnpSubsystem::ZDO, cmd::zdo::IEEE_ADDR_RSP);
            rsp.appendByte(0);      // Status
            rsp.appendQWord(device->second.ieeeAddr);
            rsp.appendWord(device->second.nwkAddr);
            rsp.appendByte(0);      // StartIndex
            rsp.appendByte(0);      // NumAssocDev
            queueFrame(rsp);
        } else if (req.command() == cmd::zdo::NWK_ADDR_REQ) {
            const uint64_t ieeeAddr = req.getQWord(0);
            for (const auto& [nwkAddr, device] : m_devices) {
                if (device.ieeeAddr != ieeeAddr) continue;
                ZnpFrame rsp(ZnpType::AREQ, ZnpSubsystem::ZDO, cmd::zdo::NWK_ADDR_RSP);
                rsp.appendByte(0);      // Status
                rsp.appendQWord(ieeeAddr);
                rsp.appendWord(nwkAddr);
                rsp.appendByte(0);      // StartIndex
                rsp.appendByte(0);      // NumAssocDev
                queueFrame(rsp);
                return;
            }
        } else if (req.command() == cmd::zdo::ACTIVE_EP_REQ) {
            auto device = m_devices.find(req.getWord(0));
            if (device == m_devices.end()) return;

            ZnpFrame rsp(ZnpType::AREQ, ZnpSubsystem::ZDO, cmd::zdo::ACTIVE_EP_RSP);
            rsp.appendWord(device->second.nwkAddr);
            rsp.appendByte(0);      // Status
            rsp.appendWord(device->second.nwkAddr);
            rsp.appendByte(static_cast<uint8_t>(device->second.inClusters.size()));
            for (const auto& [endpoint, clusters] : device->second.inClusters) {
                rsp.appendByte(endpoint);
            }
            queueFrame(rsp);
        } else if (req.command() == cmd::zdo::SIMPLE_DESC_REQ) {
            auto device = m_devices.find(req.getWord(0));
            if (device == m_devices.end()) return;

            uint8_t endpoint = req.getByte(4);
            if (m_failedEndpoints.count({device->second.nwkAddr, endpoint}) > 0) {
                ZnpFrame failure(ZnpType::AREQ, ZnpSubsystem::ZDO, cmd::zdo::SIMPLE_DESC_RSP);
                failure.appendWord(device->second.nwkAddr);
                failure.appendByte(0x80);   // Status: invalid request
                failure.appendWord(device->second.nwkAddr);
                failure.appendByte(0);      // Length
                queueOrHold(device->second.nwkAddr, endpoint, failure);
                return;
            }

            auto clusters = device->second.inClusters.find(endpoint);
            if (clusters == device->second.inClusters.end()) return;

            ZnpFrame rsp(ZnpType::AREQ, ZnpSubsystem::ZDO, cmd::zdo::SIMPLE_DESC_RSP);
            rsp.appendWord(device->second.nwkAddr);
            rsp.appendByte(0);      // Status
            rsp.appendWord(device->second.nwkAddr);
            rsp.appendByte(static_cast<uint8_t>(8 + clusters->second.size() * 2));
            rsp.appendByte(endpoint);
            rsp.appendWord(0x0104); // Home Automation profile
            rsp.appendWord(0x0100); // Device ID
            rsp.appendByte(0);      // Device version
            rsp.appendByte(static_cast<uint8_t>(clusters->second.size()));
            for (uint16_t cluster : clusters->second) {
                rsp.appendWord(cluster);
            }
            rsp.appendByte(0);      // NumOutClusters
            queueOrHold(device->second.nwkAddr, endpoint, rsp);
        }
    }

    mutable std::mutex m_mutex;
    mutable std::condition_variable m_sentCv;
    std::condition_variable m_rxCv;

    std::vector<uint8_t> m_tx;
    std::deque<uint8_t> m_rx;
    std::vector<ZnpFrame> m_sent;
    std::map<uint16_t, FakeZigbeeDevice> m_devices;
    std::map<std::pair<ZnpSubsystem, uint8_t>, Responder> m_responders;
    std::set<std::pair<uint16_t, uint8_t>> m_heldEndpoints;
    std::set<std::pair<uint16_t, uint8_t>> m_failedEndpoints;
    std::vector<std::pair<std::pair<uint16_t, uint8_t>, ZnpFrame>> m_heldFrames;
    bool m_unresponsive = false;
    size_t m_maxReadChunk = SIZE_MAX;
    uint8_t m_zclSeq = 0;
};

} // namespace smarthub::zigbee
