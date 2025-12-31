/**
 * SmartHub M4 RPMsg Implementation with VirtIO Support
 *
 * Implements proper VirtIO vring protocol for Linux interoperability.
 * Based on actual memory dump of Linux vring initialization.
 */

#include "smarthub_m4/rpmsg/rpmsg.hpp"
#include "smarthub_m4/stm32mp1xx.h"
#include "smarthub_m4/drivers/clock.hpp"
#include <cstring>

namespace smarthub::m4 {

/* Shared memory layout - from device tree */
static constexpr uint32_t VRING_TX_ADDR = 0x10040000;  // vdev0vring0 (M4->A7)
static constexpr uint32_t VRING_RX_ADDR = 0x10041000;  // vdev0vring1 (A7->M4)
static constexpr size_t VRING_NUM = 8;  // Number of vring entries
static constexpr size_t VRING_ALIGN = 16;  // From resource table

/* VirtIO vring structures */
struct vring_desc {
    uint64_t addr;      // Buffer physical address
    uint32_t len;       // Buffer length
    uint16_t flags;     // Descriptor flags
    uint16_t next;      // Next descriptor index
} __attribute__((packed));

struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VRING_NUM];
    uint16_t used_event;  // Only if VIRTIO_RING_F_EVENT_IDX
} __attribute__((packed));

struct vring_used_elem {
    uint32_t id;        // Index of start of used descriptor chain
    uint32_t len;       // Total length of descriptor chain written
} __attribute__((packed));

struct vring_used {
    uint16_t flags;
    uint16_t idx;
    vring_used_elem ring[VRING_NUM];
    uint16_t avail_event;  // Only if VIRTIO_RING_F_EVENT_IDX
} __attribute__((packed));

/* RPMsg message header */
struct rpmsg_hdr {
    uint32_t src;       // Source endpoint address
    uint32_t dst;       // Destination endpoint address
    uint32_t reserved;  // Reserved
    uint16_t len;       // Payload length
    uint16_t flags;     // Message flags
} __attribute__((packed));

/* Name service message for endpoint announcement */
struct rpmsg_ns_msg {
    char name[32];      // Service name
    uint32_t addr;      // Local endpoint address
    uint32_t flags;     // 0=create, 1=destroy
} __attribute__((packed));

static constexpr uint32_t RPMSG_NS_ADDR = 53;  // Name service endpoint

/* Calculate vring component offsets */
static constexpr size_t DESC_SIZE = sizeof(vring_desc) * VRING_NUM;  // 128 bytes
static constexpr size_t AVAIL_OFFSET = DESC_SIZE;  // 0x80
// Used ring aligned to VRING_ALIGN after avail
static constexpr size_t AVAIL_SIZE = sizeof(uint16_t) * 2 + sizeof(uint16_t) * VRING_NUM + sizeof(uint16_t);
static constexpr size_t USED_OFFSET = ((AVAIL_OFFSET + AVAIL_SIZE + VRING_ALIGN - 1) / VRING_ALIGN) * VRING_ALIGN;  // 0xA0

/* Vring pointers */
static volatile vring_desc* g_txDesc;
static volatile vring_avail* g_txAvail;
static volatile vring_used* g_txUsed;

static volatile vring_desc* g_rxDesc;
static volatile vring_avail* g_rxAvail;
static volatile vring_used* g_rxUsed;

/* Tracking indices */
static uint16_t g_txLastAvailIdx = 0;  // Last avail->idx we saw
static uint16_t g_rxLastAvailIdx = 0;  // Last RX avail->idx we processed

static constexpr uint32_t LOCAL_ADDR = 0x400;  // Our endpoint address

/* Debug trace buffer at 0x10049000 */
static volatile char* g_trace = (volatile char*)0x10049000;
static size_t g_tracePos = 0;

static void trace(const char* msg) {
    while (*msg && g_tracePos < 0x1000) {
        g_trace[g_tracePos++] = *msg++;
    }
    if (g_tracePos < 0x1000) {
        g_trace[g_tracePos++] = '\n';
    }
}

Rpmsg& Rpmsg::instance() {
    static Rpmsg instance;
    return instance;
}

bool Rpmsg::init() {
    if (m_ready) return true;

    /* Clear trace buffer first to prove new firmware is running */
    for (size_t i = 0; i < 0x200; i++) {
        g_trace[i] = 0;
    }
    g_tracePos = 0;

    trace("V3:RPMSG init");

    /* Set up vring pointers */
    g_txDesc = (volatile vring_desc*)VRING_TX_ADDR;
    g_txAvail = (volatile vring_avail*)(VRING_TX_ADDR + AVAIL_OFFSET);
    g_txUsed = (volatile vring_used*)(VRING_TX_ADDR + USED_OFFSET);

    g_rxDesc = (volatile vring_desc*)VRING_RX_ADDR;
    g_rxAvail = (volatile vring_avail*)(VRING_RX_ADDR + AVAIL_OFFSET);
    g_rxUsed = (volatile vring_used*)(VRING_RX_ADDR + USED_OFFSET);

    trace("RPMSG:vring ptrs set");

    /* Skip IPCC for now - bus fault blocks all peripheral access */
    trace("RPMSG:IPCC skip");

    /* Initialize tracking indices */
    g_txLastAvailIdx = 0;
    g_rxLastAvailIdx = g_rxAvail->idx;

    m_ready = true;
    m_lastError = nullptr;

    trace("RPMSG:waiting for buffers");

    /* Wait for Linux to provide TX buffers - use busy wait */
    uint32_t timeout = 500000;  /* Simple counter-based timeout */
    while (g_txAvail->idx == 0 && timeout > 0) {
        timeout--;
        for (volatile int i = 0; i < 100; i++) { __asm volatile("nop"); }
    }

    if (timeout == 0) {
        trace("RPMSG:timeout");
        m_lastError = "Timeout waiting for TX buffers";
        return false;
    }

    trace("RPMSG:buffers ready");

    trace("RPMSG:delay...");
    /* Small delay before name service announcement - use busy wait */
    for (volatile int i = 0; i < 100000; i++) { __asm volatile("nop"); }

    trace("RPMSG:announce");
    if (announceEndpoint()) {
        trace("RPMSG:NS sent OK");
    } else {
        trace("RPMSG:NS send FAIL");
    }

    trace("RPMSG:init done");
    return true;
}

bool Rpmsg::announceEndpoint() {
    /* Send name service announcement */
    rpmsg_ns_msg nsMsg;
    std::memset(&nsMsg, 0, sizeof(nsMsg));
    std::strncpy(nsMsg.name, "rpmsg-tty", sizeof(nsMsg.name) - 1);
    nsMsg.addr = LOCAL_ADDR;
    nsMsg.flags = 0;  // Create

    return sendRaw(LOCAL_ADDR, RPMSG_NS_ADDR, &nsMsg, sizeof(nsMsg));
}

bool Rpmsg::sendRaw(uint32_t src, uint32_t dst, const void* data, size_t len) {
    trace("sendRaw:start");
    if (!m_ready) {
        trace("sendRaw:not ready");
        m_lastError = "RPMsg not initialized";
        return false;
    }

    trace("sendRaw:check avail");
    /* Check if there are available buffers from Linux */
    uint16_t availIdx = g_txAvail->idx;
    if (g_txLastAvailIdx == availIdx) {
        trace("sendRaw:no buf");
        m_lastError = "No TX buffers available";
        return false;
    }

    trace("sendRaw:get desc");
    /* Get descriptor index from avail ring */
    uint16_t descIdx = g_txAvail->ring[g_txLastAvailIdx % VRING_NUM];
    g_txLastAvailIdx++;

    trace("sendRaw:get buf");
    /* Get buffer info from descriptor */
    volatile vring_desc* desc = &g_txDesc[descIdx];
    uint8_t* buf = (uint8_t*)(uintptr_t)desc->addr;
    uint32_t bufLen = desc->len;

    /* Check payload fits */
    size_t totalLen = sizeof(rpmsg_hdr) + len;
    if (totalLen > bufLen) {
        m_lastError = "Payload too large";
        return false;
    }

    /* Build RPMsg header */
    rpmsg_hdr* hdr = (rpmsg_hdr*)buf;
    hdr->src = src;
    hdr->dst = dst;
    hdr->reserved = 0;
    hdr->len = len;
    hdr->flags = 0;

    /* Copy payload */
    if (data && len > 0) {
        std::memcpy(buf + sizeof(rpmsg_hdr), data, len);
    }

    /* Memory barrier before updating used ring */
    __DSB();

    /* Add to used ring */
    uint16_t usedIdx = g_txUsed->idx;
    g_txUsed->ring[usedIdx % VRING_NUM].id = descIdx;
    g_txUsed->ring[usedIdx % VRING_NUM].len = totalLen;

    /* Memory barrier before updating index */
    __DSB();

    g_txUsed->idx = usedIdx + 1;

    /* Memory barrier before notification */
    __DSB();

    /* Notify host via IPCC */
    notifyHost();

    return true;
}

void Rpmsg::shutdown() {
    if (!m_ready) return;

    NVIC_DisableIRQ(IPCC_RX0_IRQn);
    m_ready = false;
}

void Rpmsg::poll() {
    if (!m_ready) return;

    /* Check for incoming messages in RX vring */
    uint16_t availIdx = g_rxAvail->idx;

    while (g_rxLastAvailIdx != availIdx) {
        /* Get descriptor index from avail ring */
        uint16_t descIdx = g_rxAvail->ring[g_rxLastAvailIdx % VRING_NUM];

        /* Get buffer from descriptor */
        volatile vring_desc* desc = &g_rxDesc[descIdx];
        const uint8_t* buf = (const uint8_t*)(uintptr_t)desc->addr;
        size_t bufLen = desc->len;

        /* Parse RPMsg header */
        if (bufLen >= sizeof(rpmsg_hdr)) {
            const rpmsg_hdr* hdr = (const rpmsg_hdr*)buf;
            const void* payload = buf + sizeof(rpmsg_hdr);
            size_t payloadLen = hdr->len;

            if (payloadLen <= bufLen - sizeof(rpmsg_hdr)) {
                /* Process the message */
                handleIncomingMessage(hdr, payload, payloadLen);
            }
        }

        /* Return buffer to used ring */
        uint16_t usedIdx = g_rxUsed->idx;
        g_rxUsed->ring[usedIdx % VRING_NUM].id = descIdx;
        g_rxUsed->ring[usedIdx % VRING_NUM].len = bufLen;

        __DSB();

        g_rxUsed->idx = usedIdx + 1;

        g_rxLastAvailIdx++;
    }
}

void Rpmsg::handleIncomingMessage(const void* /* rhdr */, const void* payload, size_t len) {
    if (len < sizeof(MsgHeader)) {
        /* Not our protocol - might be a control message */
        return;
    }

    const MsgHeader* hdr = (const MsgHeader*)payload;
    const void* msgPayload = (const uint8_t*)payload + sizeof(MsgHeader);
    size_t msgPayloadLen = hdr->len;

    if (msgPayloadLen > len - sizeof(MsgHeader)) {
        msgPayloadLen = len - sizeof(MsgHeader);
    }

    /* Handle built-in commands */
    switch ((MsgType)hdr->type) {
        case MsgType::CMD_PING:
            sendPong();
            break;

        case MsgType::CMD_GET_STATUS:
            sendStatus(Clock::getTicks(), 0, 1000);
            break;

        default:
            /* Forward to user callback */
            if (m_callback) {
                m_callback(hdr, msgPayload, msgPayloadLen);
            }
            break;
    }
}

bool Rpmsg::send(MsgType type, const void* payload, size_t len) {
    if (!m_ready) {
        m_lastError = "RPMsg not initialized";
        return false;
    }

    if (len > MAX_MSG_SIZE - sizeof(MsgHeader)) {
        m_lastError = "Payload too large";
        return false;
    }

    /* Build our message */
    uint8_t msgBuf[MAX_MSG_SIZE];
    MsgHeader* hdr = (MsgHeader*)msgBuf;
    hdr->type = (uint8_t)type;
    hdr->flags = 0;
    hdr->seq = m_seqNum++;
    hdr->len = len;
    hdr->reserved = 0;

    if (payload && len > 0) {
        std::memcpy(msgBuf + sizeof(MsgHeader), payload, len);
    }

    /* Send via RPMsg */
    return sendRaw(LOCAL_ADDR, 0, msgBuf, sizeof(MsgHeader) + len);
}

void Rpmsg::notifyHost() {
    /* IPCC access blocked - skip notification for now */
    /* TODO: Need ETZPC configuration to allow M4 IPCC access */
    trace("NOTIFY:skip");
}

bool Rpmsg::sendSensorData(uint8_t sensorId, SensorType type,
                            int32_t value, int32_t scale) {
    SensorDataPayload payload;
    payload.sensorId = sensorId;
    payload.sensorType = (uint8_t)type;
    payload.reserved = 0;
    payload.value = value;
    payload.scale = scale;
    payload.timestamp = Clock::getTicks();

    return send(MsgType::EVT_SENSOR_UPDATE, &payload, sizeof(payload));
}

bool Rpmsg::sendStatus(uint32_t uptime, uint8_t sensorCount,
                        uint16_t pollInterval) {
    StatusPayload payload;
    payload.uptime = uptime;
    payload.sensorCount = sensorCount;
    payload.errorCount = 0;
    payload.pollInterval = pollInterval;
    payload.freeMemory = 0;

    return send(MsgType::RSP_STATUS, &payload, sizeof(payload));
}

bool Rpmsg::sendPong() {
    return send(MsgType::RSP_PONG);
}

} // namespace smarthub::m4

/**
 * IPCC RX interrupt handler
 */
/* IPCC IRQ handler disabled - M4 doesn't have IPCC access currently */
/*
extern "C" void IPCC_RX0_IRQHandler(void) {
    IPCC->C2SCR = (1 << 0);
}
*/
