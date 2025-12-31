/**
 * I2C Transaction Tests
 *
 * Tests I2C operations with a mock I2C implementation.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <vector>
#include <queue>
#include <functional>

namespace {

/**
 * Mock I2C Interface for testing
 */
class MockI2C {
public:
    // Transaction record for verification
    struct Transaction {
        enum Type { PROBE, WRITE, READ };
        Type type;
        uint8_t addr;
        std::vector<uint8_t> data;
    };

    MockI2C() = default;

    // Configuration
    void setProbeResponse(uint8_t addr, bool present) {
        m_presentDevices[addr] = present;
    }

    void queueReadData(const std::vector<uint8_t>& data) {
        m_readQueue.push(data);
    }

    void setWriteSuccess(bool success) {
        m_writeSuccess = success;
    }

    // I2C operations (same interface as real I2C)
    bool probe(uint8_t addr) {
        m_transactions.push_back({Transaction::PROBE, addr, {}});
        auto it = m_presentDevices.find(addr);
        return it != m_presentDevices.end() && it->second;
    }

    bool write(uint8_t addr, const uint8_t* data, size_t len) {
        std::vector<uint8_t> writeData(data, data + len);
        m_transactions.push_back({Transaction::WRITE, addr, writeData});
        return m_writeSuccess;
    }

    bool read(uint8_t addr, uint8_t* data, size_t len) {
        m_transactions.push_back({Transaction::READ, addr, {}});

        if (m_readQueue.empty()) {
            return false;
        }

        auto& readData = m_readQueue.front();
        if (readData.size() < len) {
            return false;
        }

        std::copy(readData.begin(), readData.begin() + len, data);
        m_readQueue.pop();
        return true;
    }

    bool writeReg(uint8_t addr, uint8_t reg, uint8_t value) {
        uint8_t data[2] = {reg, value};
        return write(addr, data, 2);
    }

    bool readReg(uint8_t addr, uint8_t reg, uint8_t* value) {
        uint8_t regAddr = reg;
        if (!write(addr, &regAddr, 1)) {
            return false;
        }
        return read(addr, value, 1);
    }

    bool readRegs(uint8_t addr, uint8_t reg, uint8_t* data, size_t len) {
        uint8_t regAddr = reg;
        if (!write(addr, &regAddr, 1)) {
            return false;
        }
        return read(addr, data, len);
    }

    // Test inspection
    const std::vector<Transaction>& getTransactions() const {
        return m_transactions;
    }

    void clearTransactions() {
        m_transactions.clear();
    }

    size_t transactionCount() const {
        return m_transactions.size();
    }

private:
    std::map<uint8_t, bool> m_presentDevices;
    std::queue<std::vector<uint8_t>> m_readQueue;
    std::vector<Transaction> m_transactions;
    bool m_writeSuccess = true;
};

} // anonymous namespace

class I2CTransactionsTest : public ::testing::Test {
protected:
    void SetUp() override {
        i2c = std::make_unique<MockI2C>();
    }

    std::unique_ptr<MockI2C> i2c;
};

// ============================================================================
// Probe Tests
// ============================================================================

TEST_F(I2CTransactionsTest, Probe_DevicePresent) {
    i2c->setProbeResponse(0x44, true);
    EXPECT_TRUE(i2c->probe(0x44));
}

TEST_F(I2CTransactionsTest, Probe_DeviceNotPresent) {
    i2c->setProbeResponse(0x44, false);
    EXPECT_FALSE(i2c->probe(0x44));
}

TEST_F(I2CTransactionsTest, Probe_UnknownDevice) {
    // Device not configured - should return false
    EXPECT_FALSE(i2c->probe(0x99));
}

TEST_F(I2CTransactionsTest, Probe_MultipleDevices) {
    i2c->setProbeResponse(0x44, true);   // SHT31 default
    i2c->setProbeResponse(0x45, true);   // SHT31 alternate
    i2c->setProbeResponse(0x76, false);  // BME280 not present

    EXPECT_TRUE(i2c->probe(0x44));
    EXPECT_TRUE(i2c->probe(0x45));
    EXPECT_FALSE(i2c->probe(0x76));
}

TEST_F(I2CTransactionsTest, Probe_TransactionRecorded) {
    i2c->setProbeResponse(0x44, true);
    i2c->probe(0x44);

    EXPECT_EQ(i2c->transactionCount(), 1);
    EXPECT_EQ(i2c->getTransactions()[0].type, MockI2C::Transaction::PROBE);
    EXPECT_EQ(i2c->getTransactions()[0].addr, 0x44);
}

// ============================================================================
// Write Tests
// ============================================================================

TEST_F(I2CTransactionsTest, Write_SingleByte) {
    i2c->setWriteSuccess(true);
    uint8_t data[] = {0x30};
    EXPECT_TRUE(i2c->write(0x44, data, 1));

    EXPECT_EQ(i2c->transactionCount(), 1);
    auto& tx = i2c->getTransactions()[0];
    EXPECT_EQ(tx.type, MockI2C::Transaction::WRITE);
    EXPECT_EQ(tx.addr, 0x44);
    EXPECT_EQ(tx.data.size(), 1);
    EXPECT_EQ(tx.data[0], 0x30);
}

TEST_F(I2CTransactionsTest, Write_MultipleBytes) {
    i2c->setWriteSuccess(true);
    uint8_t data[] = {0x24, 0x00};  // SHT31 measurement command
    EXPECT_TRUE(i2c->write(0x44, data, 2));

    auto& tx = i2c->getTransactions()[0];
    EXPECT_EQ(tx.data.size(), 2);
    EXPECT_EQ(tx.data[0], 0x24);
    EXPECT_EQ(tx.data[1], 0x00);
}

TEST_F(I2CTransactionsTest, Write_Failure) {
    i2c->setWriteSuccess(false);
    uint8_t data[] = {0x24, 0x00};
    EXPECT_FALSE(i2c->write(0x44, data, 2));
}

TEST_F(I2CTransactionsTest, WriteReg_Success) {
    i2c->setWriteSuccess(true);
    EXPECT_TRUE(i2c->writeReg(0x44, 0x30, 0xA2));

    auto& tx = i2c->getTransactions()[0];
    EXPECT_EQ(tx.type, MockI2C::Transaction::WRITE);
    EXPECT_EQ(tx.data.size(), 2);
    EXPECT_EQ(tx.data[0], 0x30);  // register
    EXPECT_EQ(tx.data[1], 0xA2);  // value
}

// ============================================================================
// Read Tests
// ============================================================================

TEST_F(I2CTransactionsTest, Read_SingleByte) {
    i2c->queueReadData({0xAB});
    uint8_t data;
    EXPECT_TRUE(i2c->read(0x44, &data, 1));
    EXPECT_EQ(data, 0xAB);
}

TEST_F(I2CTransactionsTest, Read_MultipleBytes) {
    i2c->queueReadData({0x64, 0x8C, 0x92, 0x9C, 0xA5, 0xB3});
    uint8_t data[6];
    EXPECT_TRUE(i2c->read(0x44, data, 6));

    EXPECT_EQ(data[0], 0x64);
    EXPECT_EQ(data[1], 0x8C);
    EXPECT_EQ(data[2], 0x92);
    EXPECT_EQ(data[3], 0x9C);
    EXPECT_EQ(data[4], 0xA5);
    EXPECT_EQ(data[5], 0xB3);
}

TEST_F(I2CTransactionsTest, Read_EmptyQueue) {
    uint8_t data;
    EXPECT_FALSE(i2c->read(0x44, &data, 1));
}

TEST_F(I2CTransactionsTest, Read_InsufficientData) {
    i2c->queueReadData({0x01, 0x02});
    uint8_t data[6];
    EXPECT_FALSE(i2c->read(0x44, data, 6));
}

TEST_F(I2CTransactionsTest, Read_TransactionRecorded) {
    i2c->queueReadData({0xAB});
    uint8_t data;
    i2c->read(0x44, &data, 1);

    EXPECT_EQ(i2c->transactionCount(), 1);
    EXPECT_EQ(i2c->getTransactions()[0].type, MockI2C::Transaction::READ);
    EXPECT_EQ(i2c->getTransactions()[0].addr, 0x44);
}

// ============================================================================
// Register Read Tests
// ============================================================================

TEST_F(I2CTransactionsTest, ReadReg_Success) {
    i2c->setWriteSuccess(true);
    i2c->queueReadData({0x55});

    uint8_t value;
    EXPECT_TRUE(i2c->readReg(0x44, 0x0F, &value));
    EXPECT_EQ(value, 0x55);

    // Should have write (register address) + read
    EXPECT_EQ(i2c->transactionCount(), 2);
}

TEST_F(I2CTransactionsTest, ReadRegs_Success) {
    i2c->setWriteSuccess(true);
    i2c->queueReadData({0x01, 0x02, 0x03, 0x04});

    uint8_t data[4];
    EXPECT_TRUE(i2c->readRegs(0x44, 0x00, data, 4));

    EXPECT_EQ(data[0], 0x01);
    EXPECT_EQ(data[1], 0x02);
    EXPECT_EQ(data[2], 0x03);
    EXPECT_EQ(data[3], 0x04);
}

TEST_F(I2CTransactionsTest, ReadReg_WriteFailure) {
    i2c->setWriteSuccess(false);
    i2c->queueReadData({0x55});

    uint8_t value;
    EXPECT_FALSE(i2c->readReg(0x44, 0x0F, &value));
}

// ============================================================================
// SHT31-Specific Transaction Tests
// ============================================================================

TEST_F(I2CTransactionsTest, SHT31_ProbeAndInit) {
    i2c->setProbeResponse(0x44, true);
    i2c->setWriteSuccess(true);

    // Simulate SHT31 initialization
    EXPECT_TRUE(i2c->probe(0x44));

    // Soft reset command: 0x30A2
    uint8_t resetCmd[] = {0x30, 0xA2};
    EXPECT_TRUE(i2c->write(0x44, resetCmd, 2));

    // Clear status command: 0x3041
    uint8_t clearCmd[] = {0x30, 0x41};
    EXPECT_TRUE(i2c->write(0x44, clearCmd, 2));

    EXPECT_EQ(i2c->transactionCount(), 3);
}

TEST_F(I2CTransactionsTest, SHT31_Measurement) {
    i2c->setProbeResponse(0x44, true);
    i2c->setWriteSuccess(true);

    // Queue measurement data: temp MSB, temp LSB, temp CRC, hum MSB, hum LSB, hum CRC
    i2c->queueReadData({0x64, 0x8C, 0x92, 0x9C, 0xA5, 0xB3});

    // Send measurement command: 0x2400 (high repeatability)
    uint8_t measCmd[] = {0x24, 0x00};
    EXPECT_TRUE(i2c->write(0x44, measCmd, 2));

    // Read 6 bytes
    uint8_t data[6];
    EXPECT_TRUE(i2c->read(0x44, data, 6));

    // Verify data
    EXPECT_EQ(data[0], 0x64);  // temp MSB
    EXPECT_EQ(data[1], 0x8C);  // temp LSB
    EXPECT_EQ(data[3], 0x9C);  // hum MSB
    EXPECT_EQ(data[4], 0xA5);  // hum LSB
}

TEST_F(I2CTransactionsTest, SHT31_ReadStatus) {
    i2c->setWriteSuccess(true);
    i2c->queueReadData({0x80, 0x10, 0xAB});  // Status + CRC

    // Send read status command: 0xF32D
    uint8_t statusCmd[] = {0xF3, 0x2D};
    EXPECT_TRUE(i2c->write(0x44, statusCmd, 2));

    // Read 3 bytes (status MSB, LSB, CRC)
    uint8_t data[3];
    EXPECT_TRUE(i2c->read(0x44, data, 3));

    uint16_t status = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    EXPECT_EQ(status, 0x8010);
}

// ============================================================================
// Multiple Device Tests
// ============================================================================

TEST_F(I2CTransactionsTest, MultipleDevices_ScanBus) {
    // Simulate I2C bus scan
    i2c->setProbeResponse(0x44, true);   // SHT31
    i2c->setProbeResponse(0x76, true);   // BME280
    i2c->setProbeResponse(0x68, false);  // MPU6050 not present

    std::vector<uint8_t> foundDevices;
    uint8_t addresses[] = {0x44, 0x45, 0x68, 0x76, 0x77};

    for (uint8_t addr : addresses) {
        if (i2c->probe(addr)) {
            foundDevices.push_back(addr);
        }
    }

    EXPECT_EQ(foundDevices.size(), 2);
    EXPECT_EQ(foundDevices[0], 0x44);
    EXPECT_EQ(foundDevices[1], 0x76);
}

TEST_F(I2CTransactionsTest, MultipleReads_Sequential) {
    i2c->queueReadData({0x11, 0x22});
    i2c->queueReadData({0x33, 0x44});
    i2c->queueReadData({0x55, 0x66});

    uint8_t data[2];

    EXPECT_TRUE(i2c->read(0x44, data, 2));
    EXPECT_EQ(data[0], 0x11);
    EXPECT_EQ(data[1], 0x22);

    EXPECT_TRUE(i2c->read(0x44, data, 2));
    EXPECT_EQ(data[0], 0x33);
    EXPECT_EQ(data[1], 0x44);

    EXPECT_TRUE(i2c->read(0x44, data, 2));
    EXPECT_EQ(data[0], 0x55);
    EXPECT_EQ(data[1], 0x66);

    // Queue exhausted
    EXPECT_FALSE(i2c->read(0x44, data, 2));
}

// ============================================================================
// Transaction History Tests
// ============================================================================

TEST_F(I2CTransactionsTest, TransactionHistory_Complete) {
    i2c->setProbeResponse(0x44, true);
    i2c->setWriteSuccess(true);
    i2c->queueReadData({0xAB, 0xCD});

    // Perform sequence of operations
    i2c->probe(0x44);
    uint8_t writeData[] = {0x24, 0x00};
    i2c->write(0x44, writeData, 2);
    uint8_t readData[2];
    i2c->read(0x44, readData, 2);

    // Verify transaction history
    auto& txs = i2c->getTransactions();
    EXPECT_EQ(txs.size(), 3);

    EXPECT_EQ(txs[0].type, MockI2C::Transaction::PROBE);
    EXPECT_EQ(txs[0].addr, 0x44);

    EXPECT_EQ(txs[1].type, MockI2C::Transaction::WRITE);
    EXPECT_EQ(txs[1].addr, 0x44);
    EXPECT_EQ(txs[1].data, std::vector<uint8_t>({0x24, 0x00}));

    EXPECT_EQ(txs[2].type, MockI2C::Transaction::READ);
    EXPECT_EQ(txs[2].addr, 0x44);
}

TEST_F(I2CTransactionsTest, TransactionHistory_Clear) {
    i2c->setProbeResponse(0x44, true);
    i2c->probe(0x44);
    EXPECT_EQ(i2c->transactionCount(), 1);

    i2c->clearTransactions();
    EXPECT_EQ(i2c->transactionCount(), 0);
}
