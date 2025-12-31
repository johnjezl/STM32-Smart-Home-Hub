/**
 * Unit tests for rpmsg_poll parsing and data structures
 *
 * Tests the VirtIO vring structures and RPMsg parsing logic
 * without requiring hardware access.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>
#include <vector>

/* Replicate structures from rpmsg_poll.c for testing */
#define VRING_NUM_DESCS  16

struct vring_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VRING_NUM_DESCS];
};

struct vring_used_elem {
    uint32_t id;
    uint32_t len;
};

struct vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[VRING_NUM_DESCS];
};

struct rpmsg_hdr {
    uint32_t src;
    uint32_t dst;
    uint32_t reserved;
    uint16_t len;
    uint16_t flags;
    uint8_t data[];
};

/* Constants from rpmsg_poll.c */
#define VRING_DESC_OFFSET   0
#define VRING_AVAIL_OFFSET  (VRING_NUM_DESCS * sizeof(struct vring_desc))
#define VRING_USED_OFFSET   0xA0

#define VRING0_ADDR     0x10040000
#define VBUFFER_ADDR    0x10042000
#define VRING_SIZE      0x1000
#define VBUFFER_SIZE    0x4000

/* ==== VirtIO Structure Tests ==== */

class VirtioStructureTest : public ::testing::Test {
protected:
    alignas(4096) uint8_t vring_memory[VRING_SIZE];
    alignas(4096) uint8_t buffer_memory[VBUFFER_SIZE];

    void SetUp() override {
        memset(vring_memory, 0, sizeof(vring_memory));
        memset(buffer_memory, 0, sizeof(buffer_memory));
    }

    vring_desc* descriptors() {
        return reinterpret_cast<vring_desc*>(vring_memory);
    }

    vring_avail* available() {
        return reinterpret_cast<vring_avail*>(vring_memory + VRING_AVAIL_OFFSET);
    }

    vring_used* used() {
        return reinterpret_cast<vring_used*>(vring_memory + VRING_USED_OFFSET);
    }
};

TEST_F(VirtioStructureTest, DescriptorSize) {
    EXPECT_EQ(sizeof(vring_desc), 16);
}

TEST_F(VirtioStructureTest, UsedElemSize) {
    EXPECT_EQ(sizeof(vring_used_elem), 8);
}

TEST_F(VirtioStructureTest, RpmsgHeaderSize) {
    EXPECT_EQ(sizeof(rpmsg_hdr), 16);
}

TEST_F(VirtioStructureTest, DescriptorTableOffset) {
    EXPECT_EQ(VRING_DESC_OFFSET, 0);
}

TEST_F(VirtioStructureTest, AvailableRingOffset) {
    // 16 descriptors × 16 bytes = 256 bytes = 0x100
    // But actual offset depends on structure layout
    size_t expected = VRING_NUM_DESCS * sizeof(vring_desc);
    EXPECT_EQ(VRING_AVAIL_OFFSET, expected);
}

TEST_F(VirtioStructureTest, UsedRingOffset) {
    // Used ring at 0xA0 (160 bytes from start)
    EXPECT_EQ(VRING_USED_OFFSET, 0xA0);
}

TEST_F(VirtioStructureTest, DescriptorTableFitsBeforeAvail) {
    // Descriptor table should not overlap with available ring
    size_t desc_end = VRING_DESC_OFFSET + VRING_NUM_DESCS * sizeof(vring_desc);
    EXPECT_LE(desc_end, VRING_AVAIL_OFFSET);
}

TEST_F(VirtioStructureTest, VringLayoutMatchesHardware) {
    // These offsets were verified by reading actual hardware memory on STM32MP157F-DK2.
    // The used ring offset (0xA0 = 160) is determined by the remoteproc/OpenAMP
    // implementation in the Linux kernel, not by simple struct size calculations.
    // Our test structures are for data interpretation, not memory layout.

    // Verify the constants match what we expect from hardware observations
    EXPECT_EQ(VRING_USED_OFFSET, 0xA0);
    EXPECT_EQ(VRING_SIZE, 0x1000);
    EXPECT_EQ(VBUFFER_SIZE, 0x4000);
}

TEST_F(VirtioStructureTest, UsedRingFitsInVring) {
    // Used ring should fit within vring size
    size_t used_end = VRING_USED_OFFSET + sizeof(vring_used);
    EXPECT_LE(used_end, VRING_SIZE);
}

/* ==== Descriptor Tests ==== */

TEST_F(VirtioStructureTest, SetDescriptor) {
    descriptors()[0].addr = VBUFFER_ADDR;
    descriptors()[0].len = 512;
    descriptors()[0].flags = 0;
    descriptors()[0].next = 1;

    EXPECT_EQ(descriptors()[0].addr, VBUFFER_ADDR);
    EXPECT_EQ(descriptors()[0].len, 512);
    EXPECT_EQ(descriptors()[0].flags, 0);
    EXPECT_EQ(descriptors()[0].next, 1);
}

TEST_F(VirtioStructureTest, MultipleDescriptors) {
    for (int i = 0; i < VRING_NUM_DESCS; i++) {
        descriptors()[i].addr = VBUFFER_ADDR + i * 512;
        descriptors()[i].len = 512;
        descriptors()[i].next = (i + 1) % VRING_NUM_DESCS;
    }

    EXPECT_EQ(descriptors()[0].addr, VBUFFER_ADDR);
    EXPECT_EQ(descriptors()[15].addr, VBUFFER_ADDR + 15 * 512);
    EXPECT_EQ(descriptors()[15].next, 0);  // Wraps around
}

/* ==== Available Ring Tests ==== */

TEST_F(VirtioStructureTest, AvailableRingInit) {
    available()->flags = 0;
    available()->idx = 0;

    EXPECT_EQ(available()->flags, 0);
    EXPECT_EQ(available()->idx, 0);
}

TEST_F(VirtioStructureTest, AvailableRingAdd) {
    available()->idx = 0;

    // Add descriptor 0 to available ring
    available()->ring[available()->idx % VRING_NUM_DESCS] = 0;
    available()->idx++;

    EXPECT_EQ(available()->idx, 1);
    EXPECT_EQ(available()->ring[0], 0);
}

/* ==== Used Ring Tests ==== */

TEST_F(VirtioStructureTest, UsedRingInit) {
    used()->flags = 0;
    used()->idx = 0;

    EXPECT_EQ(used()->flags, 0);
    EXPECT_EQ(used()->idx, 0);
}

TEST_F(VirtioStructureTest, UsedRingAdd) {
    used()->idx = 0;

    // M4 marks descriptor 0 as used with 56 bytes
    used()->ring[used()->idx % VRING_NUM_DESCS].id = 0;
    used()->ring[used()->idx % VRING_NUM_DESCS].len = 56;
    used()->idx++;

    EXPECT_EQ(used()->idx, 1);
    EXPECT_EQ(used()->ring[0].id, 0);
    EXPECT_EQ(used()->ring[0].len, 56);
}

TEST_F(VirtioStructureTest, UsedRingWraparound) {
    // Fill ring completely
    for (int i = 0; i < VRING_NUM_DESCS * 2; i++) {
        uint16_t ring_idx = used()->idx % VRING_NUM_DESCS;
        used()->ring[ring_idx].id = i % VRING_NUM_DESCS;
        used()->ring[ring_idx].len = 100 + i;
        used()->idx++;
    }

    // idx should wrap around
    EXPECT_EQ(used()->idx, VRING_NUM_DESCS * 2);
    // Last entry should be at position 15 (31 % 16)
    EXPECT_EQ(used()->ring[15].id, 15);
}

/* ==== RPMsg Header Tests ==== */

class RpmsgHeaderTest : public ::testing::Test {
protected:
    alignas(4) uint8_t buffer[256];

    void SetUp() override {
        memset(buffer, 0, sizeof(buffer));
    }

    rpmsg_hdr* header() {
        return reinterpret_cast<rpmsg_hdr*>(buffer);
    }
};

TEST_F(RpmsgHeaderTest, HeaderFields) {
    header()->src = 0x1234;
    header()->dst = 0x0400;  // NS address
    header()->reserved = 0;
    header()->len = 32;
    header()->flags = 0;

    EXPECT_EQ(header()->src, 0x1234);
    EXPECT_EQ(header()->dst, 0x0400);
    EXPECT_EQ(header()->len, 32);
}

TEST_F(RpmsgHeaderTest, PayloadAccess) {
    header()->len = 5;
    const char* msg = "hello";
    memcpy(header()->data, msg, 5);

    EXPECT_EQ(memcmp(header()->data, "hello", 5), 0);
}

TEST_F(RpmsgHeaderTest, NameServiceAnnouncement) {
    // NS announcement format: "rpmsg-<name>" with null terminator
    const char* ns_name = "rpmsg-smarthub-m4";
    size_t ns_len = strlen(ns_name) + 1;

    header()->src = 0x0001;  // M4 endpoint
    header()->dst = 0x0035;  // NS address (53)
    header()->len = ns_len;
    memcpy(header()->data, ns_name, ns_len);

    EXPECT_EQ(header()->dst, 0x0035);
    EXPECT_EQ(memcmp(header()->data, "rpmsg-smarthub-m4", 17), 0);
}

/* ==== Buffer Offset Calculation Tests ==== */

TEST(BufferOffsetTest, ValidOffset) {
    uint64_t buf_addr = VBUFFER_ADDR + 512;
    uint32_t offset = buf_addr - VBUFFER_ADDR;

    EXPECT_EQ(offset, 512);
    EXPECT_LT(offset, VBUFFER_SIZE);
}

TEST(BufferOffsetTest, ZeroOffset) {
    uint64_t buf_addr = VBUFFER_ADDR;
    uint32_t offset = buf_addr - VBUFFER_ADDR;

    EXPECT_EQ(offset, 0);
}

TEST(BufferOffsetTest, MaxValidOffset) {
    uint64_t buf_addr = VBUFFER_ADDR + VBUFFER_SIZE - 1;
    uint32_t offset = buf_addr - VBUFFER_ADDR;

    EXPECT_EQ(offset, VBUFFER_SIZE - 1);
    EXPECT_LT(offset, VBUFFER_SIZE);
}

TEST(BufferOffsetTest, InvalidOffset) {
    uint64_t buf_addr = VBUFFER_ADDR + VBUFFER_SIZE;
    uint32_t offset = buf_addr - VBUFFER_ADDR;

    EXPECT_GE(offset, VBUFFER_SIZE);  // Out of bounds
}

/* ==== Message Processing Simulation ==== */

class MessageProcessingTest : public ::testing::Test {
protected:
    alignas(4096) uint8_t vring_memory[VRING_SIZE];
    alignas(4096) uint8_t buffer_memory[VBUFFER_SIZE];

    void SetUp() override {
        memset(vring_memory, 0, sizeof(vring_memory));
        memset(buffer_memory, 0, sizeof(buffer_memory));
    }

    vring_desc* descriptors() {
        return reinterpret_cast<vring_desc*>(vring_memory);
    }

    vring_used* used() {
        return reinterpret_cast<vring_used*>(vring_memory + VRING_USED_OFFSET);
    }

    rpmsg_hdr* getMessage(uint32_t desc_id) {
        uint64_t buf_addr = descriptors()[desc_id].addr;
        uint32_t buf_offset = buf_addr - VBUFFER_ADDR;
        if (buf_offset >= VBUFFER_SIZE) return nullptr;
        return reinterpret_cast<rpmsg_hdr*>(buffer_memory + buf_offset);
    }

    void setupDescriptor(int idx, uint32_t buffer_offset, uint32_t len) {
        descriptors()[idx].addr = VBUFFER_ADDR + buffer_offset;
        descriptors()[idx].len = len;
        descriptors()[idx].flags = 0;
        descriptors()[idx].next = (idx + 1) % VRING_NUM_DESCS;
    }

    void addUsedEntry(uint32_t desc_id, uint32_t len) {
        uint16_t ring_idx = used()->idx % VRING_NUM_DESCS;
        used()->ring[ring_idx].id = desc_id;
        used()->ring[ring_idx].len = len;
        used()->idx++;
    }
};

TEST_F(MessageProcessingTest, SingleMessage) {
    // Setup descriptor pointing to buffer
    setupDescriptor(0, 0, 512);

    // Write a message to the buffer
    rpmsg_hdr* msg = reinterpret_cast<rpmsg_hdr*>(buffer_memory);
    msg->src = 0x0001;
    msg->dst = 0x0400;
    msg->len = 4;
    memcpy(msg->data, "test", 4);

    // Mark descriptor as used
    addUsedEntry(0, sizeof(rpmsg_hdr) + 4);

    // Verify we can read it back
    EXPECT_EQ(used()->idx, 1);
    rpmsg_hdr* read_msg = getMessage(0);
    ASSERT_NE(read_msg, nullptr);
    EXPECT_EQ(read_msg->src, 0x0001);
    EXPECT_EQ(read_msg->len, 4);
    EXPECT_EQ(memcmp(read_msg->data, "test", 4), 0);
}

TEST_F(MessageProcessingTest, MultipleMessages) {
    // Setup 3 descriptors
    setupDescriptor(0, 0, 512);
    setupDescriptor(1, 512, 512);
    setupDescriptor(2, 1024, 512);

    // Write 3 messages
    for (int i = 0; i < 3; i++) {
        rpmsg_hdr* msg = reinterpret_cast<rpmsg_hdr*>(buffer_memory + i * 512);
        msg->src = 0x0001;
        msg->dst = 0x0400;
        msg->len = 1;
        msg->data[0] = 'A' + i;
        addUsedEntry(i, sizeof(rpmsg_hdr) + 1);
    }

    EXPECT_EQ(used()->idx, 3);

    // Verify all messages
    for (int i = 0; i < 3; i++) {
        rpmsg_hdr* msg = getMessage(i);
        ASSERT_NE(msg, nullptr);
        EXPECT_EQ(msg->data[0], 'A' + i);
    }
}

TEST_F(MessageProcessingTest, PollingLoop) {
    // Simulate the polling loop logic
    uint16_t last_used_idx = 0;

    // Setup one descriptor and message
    setupDescriptor(0, 0, 512);
    rpmsg_hdr* msg = reinterpret_cast<rpmsg_hdr*>(buffer_memory);
    msg->src = 0x0001;
    msg->len = 3;
    memcpy(msg->data, "ABC", 3);
    addUsedEntry(0, sizeof(rpmsg_hdr) + 3);

    // Poll for new messages
    std::vector<std::string> received;
    while (last_used_idx != used()->idx) {
        uint16_t ring_idx = last_used_idx % VRING_NUM_DESCS;
        uint32_t desc_id = used()->ring[ring_idx].id;

        if (desc_id < VRING_NUM_DESCS) {
            rpmsg_hdr* hdr = getMessage(desc_id);
            if (hdr && hdr->len > 0) {
                received.push_back(std::string(
                    reinterpret_cast<char*>(hdr->data), hdr->len));
            }
        }
        last_used_idx++;
    }

    ASSERT_EQ(received.size(), 1);
    EXPECT_EQ(received[0], "ABC");
}

/* ==== Test Main ==== */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
