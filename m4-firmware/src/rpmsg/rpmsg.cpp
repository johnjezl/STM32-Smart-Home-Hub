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

/* From resource_table.c. Linux sets DRIVER_OK once the vrings are initialized and the name
 * service is listening. Until then they hold whatever the last firmware left there. */
extern "C" uint8_t rsc_vdev_status(void);
static constexpr uint8_t VIRTIO_CONFIG_S_DRIVER_OK = 4;

static constexpr uint16_t VRING_AVAIL_F_NO_INTERRUPT = 1;

/* IPCC channels, as Linux's stm32_rproc uses them. Each vring has its own. */
static constexpr uint32_t CHANNEL_TX_VRING = 0;
static constexpr uint32_t CHANNEL_RX_VRING = 1;
static constexpr uint32_t KICK_CHANNELS = (1u << CHANNEL_TX_VRING) | (1u << CHANNEL_RX_VRING);
static constexpr uint32_t STOP_CHANNELS = (1u << 2) | (1u << 3);  /* Shutdown, detach */

/* The M4's own enable for IPCC's clock. Direct, as stm32mp1xx.h's RCC layout is wrong (see #18). */
#define RCC_MC_AHB3ENSETR (*(volatile uint32_t*)0x50000AA0)
static constexpr uint32_t RCC_MC_AHB3ENSETR_IPCCEN = 1u << 12;

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

    trace("RPMSG:init");

    /* Set up vring pointers */
    g_txDesc = (volatile vring_desc*)VRING_TX_ADDR;
    g_txAvail = (volatile vring_avail*)(VRING_TX_ADDR + AVAIL_OFFSET);
    g_txUsed = (volatile vring_used*)(VRING_TX_ADDR + USED_OFFSET);

    g_rxDesc = (volatile vring_desc*)VRING_RX_ADDR;
    g_rxAvail = (volatile vring_avail*)(VRING_RX_ADDR + AVAIL_OFFSET);
    g_rxUsed = (volatile vring_used*)(VRING_RX_ADDR + USED_OFFSET);

    trace("RPMSG:vring ptrs set");

    /* The M4 has its own enable for IPCC's clock. Linux's enable has been enough on the board,
     * but this way the M4's use of IPCC doesn't depend on Linux keeping its own on. */
    RCC_MC_AHB3ENSETR = RCC_MC_AHB3ENSETR_IPCCEN;
    __DSB();

    /* Linux starts both vrings from zero. poll() announces us once they're ready. */
    g_txLastAvailIdx = 0;
    g_rxLastAvailIdx = 0;

    m_ready = true;
    m_lastError = nullptr;

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
    if (!m_ready) {
        trace("sendRaw:not ready");
        m_lastError = "RPMsg not initialized";
        return false;
    }

    /* Check if there are available buffers from Linux */
    uint16_t availIdx = g_txAvail->idx;
    if (g_txLastAvailIdx == availIdx) {
        trace("sendRaw:no buf");
        m_lastError = "No TX buffers available";
        return false;
    }

    /* Get descriptor index from avail ring */
    uint16_t descIdx = g_txAvail->ring[g_txLastAvailIdx % VRING_NUM];

    /* Get buffer info from descriptor */
    volatile vring_desc* desc = &g_txDesc[descIdx];
    uint8_t* buf = (uint8_t*)(uintptr_t)desc->addr;
    uint32_t bufLen = desc->len;

    /* Check payload fits. The buffer stays in the ring for a message that does. */
    size_t totalLen = sizeof(rpmsg_hdr) + len;
    if (totalLen > bufLen) {
        m_lastError = "Payload too large";
        return false;
    }
    g_txLastAvailIdx++;

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
    notifyHost(CHANNEL_TX_VRING);

    return true;
}

void Rpmsg::shutdown() {
    if (!m_ready) return;

    NVIC_DisableIRQ(IPCC_RX0_IRQn);
    m_ready = false;
}

void Rpmsg::poll() {
    if (!m_ready) return;

    const uint32_t incoming = IPCC->C1TOC2SR;

    /* Linux waits for this to be acknowledged before taking the vrings away, so leave them first */
    if (incoming & STOP_CHANNELS) {
        trace("RPMSG:stop");
        shutdown();
        IPCC->C2SCR = incoming & (KICK_CHANNELS | STOP_CHANNELS);
        return;
    }

    /* Linux can't kick us again until each kick is acknowledged */
    if (incoming & KICK_CHANNELS) {
        IPCC->C2SCR = incoming & KICK_CHANNELS;
    }

    ringPending();

    if ((rsc_vdev_status() & VIRTIO_CONFIG_S_DRIVER_OK) == 0) {
        return;
    }

    /* Tried again each time until it gets through, as it fails if there's no buffer yet */
    if (!m_announced) {
        m_announced = announceEndpoint();
        trace(m_announced ? "RPMSG:NS sent OK" : "RPMSG:NS send FAIL");
    }

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

        __DSB();

        /* So that Linux can reuse the buffer, if it's waiting on one. It usually isn't. */
        if ((g_rxAvail->flags & VRING_AVAIL_F_NO_INTERRUPT) == 0) {
            notifyHost(CHANNEL_RX_VRING);
        }
    }
}

void Rpmsg::handleIncomingMessage(const void* rhdr, const void* payload, size_t len) {
    const rpmsg_hdr* rpmsgHdr = static_cast<const rpmsg_hdr*>(rhdr);
    if (rpmsgHdr->dst != LOCAL_ADDR) {
        return;
    }

    protocol::DecodedMessage message;
    if (protocol::decodeMessage((const uint8_t*)payload, len, message) != protocol::DecodeResult::Ok) {
        /* Not our protocol - might be a control message */
        return;
    }

    /* Replies go to whoever last wrote to us: Linux gives its end an address when it's opened */
    m_hostAddr = rpmsgHdr->src;

    const MsgHeader* hdr = &message.header;
    const void* msgPayload = message.payload;
    size_t msgPayloadLen = message.header.len;

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
    if (m_hostAddr == 0) {
        m_lastError = "No host endpoint yet";  /* Nothing has written to us, so nobody's listening */
        return false;
    }

    /* Build our message */
    uint8_t msgBuf[MAX_MSG_SIZE];
    const size_t total = protocol::encodeMessage(type, m_seqNum, payload, len, msgBuf, sizeof(msgBuf));
    if (total == 0) {
        m_lastError = "Payload too large";
        return false;
    }
    m_seqNum++;

    /* Send via RPMsg */
    return sendRaw(LOCAL_ADDR, m_hostAddr, msgBuf, total);
}

void Rpmsg::notifyHost(uint32_t channel) {
    m_pendingNotify |= 1u << channel;
    ringPending();
}

void Rpmsg::ringPending() {
    /* A channel Linux hasn't cleared yet may be one it has already looked at the vring for, so
     * this can't count as told. It's rung once it's free, on a later poll if need be. */
    const uint32_t ready = m_pendingNotify & ~IPCC->C2TOC1SR;
    if (ready != 0) {
        IPCC->C2SCR = ready << 16;
        m_pendingNotify &= ~ready;
        __DSB();
    }
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
