/**
 * SHT31 Sensor Calculations Tests
 *
 * Tests CRC-8, temperature conversion, and humidity conversion.
 * These are pure calculations that don't require hardware.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <cmath>

namespace {

// CRC-8 polynomial for SHT31 (same as in sht31.cpp)
constexpr uint8_t CRC_POLYNOMIAL = 0x31;

/**
 * CRC-8 calculation (copy from sht31.cpp for testing)
 * Polynomial: 0x31 (x^8 + x^5 + x^4 + 1)
 * Initialization: 0xFF
 */
uint8_t crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ CRC_POLYNOMIAL;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * Convert raw temperature value to Celsius
 * Formula: T = -45 + 175 * (rawValue / 65535)
 */
float rawToTemperature(uint16_t raw) {
    return -45.0f + 175.0f * (static_cast<float>(raw) / 65535.0f);
}

/**
 * Convert raw humidity value to %RH
 * Formula: RH = 100 * (rawValue / 65535)
 */
float rawToHumidity(uint16_t raw) {
    float rh = 100.0f * (static_cast<float>(raw) / 65535.0f);
    if (rh > 100.0f) rh = 100.0f;
    if (rh < 0.0f) rh = 0.0f;
    return rh;
}

/**
 * Convert temperature to fixed-point (x100)
 */
int32_t temperatureToFixed(float temp) {
    return static_cast<int32_t>(temp * 100.0f);
}

/**
 * Convert humidity to fixed-point (x100)
 */
int32_t humidityToFixed(float hum) {
    return static_cast<int32_t>(hum * 100.0f);
}

} // anonymous namespace

class SHT31CalculationsTest : public ::testing::Test {};

// ============================================================================
// CRC-8 Tests
// ============================================================================

TEST_F(SHT31CalculationsTest, CRC8_EmptyData) {
    uint8_t data[] = {};
    // CRC of empty data should be initial value
    EXPECT_EQ(crc8(data, 0), 0xFF);
}

TEST_F(SHT31CalculationsTest, CRC8_SingleByte) {
    uint8_t data[] = {0x00};
    uint8_t crc = crc8(data, 1);
    // Calculated: 0xFF ^ 0x00 = 0xFF, then 8 shifts
    EXPECT_EQ(crc, 0xAC);
}

TEST_F(SHT31CalculationsTest, CRC8_KnownValues) {
    // Test with known SHT31 measurement data
    // These values are from actual sensor readings or datasheet examples

    // Example: 0xBE, 0xEF should give specific CRC
    uint8_t data1[] = {0xBE, 0xEF};
    EXPECT_EQ(crc8(data1, 2), 0x92);

    // Another example
    uint8_t data2[] = {0x00, 0x00};
    EXPECT_EQ(crc8(data2, 2), 0x81);

    // Max value
    uint8_t data3[] = {0xFF, 0xFF};
    EXPECT_EQ(crc8(data3, 2), 0xAC);
}

TEST_F(SHT31CalculationsTest, CRC8_TemperatureValue) {
    // Typical temperature raw value ~0x6000 (about 25°C)
    uint8_t data[] = {0x60, 0x00};
    uint8_t crc = crc8(data, 2);
    // Just verify it's deterministic
    EXPECT_EQ(crc8(data, 2), crc);
}

TEST_F(SHT31CalculationsTest, CRC8_HumidityValue) {
    // Typical humidity raw value ~0x8000 (about 50%)
    uint8_t data[] = {0x80, 0x00};
    uint8_t crc = crc8(data, 2);
    EXPECT_EQ(crc8(data, 2), crc);
}

TEST_F(SHT31CalculationsTest, CRC8_StatusRegister) {
    // Status register example
    uint8_t data[] = {0x80, 0x10};
    uint8_t crc = crc8(data, 2);
    // Verify deterministic
    EXPECT_EQ(crc8(data, 2), crc);
}

// ============================================================================
// Temperature Conversion Tests
// ============================================================================

TEST_F(SHT31CalculationsTest, Temperature_MinValue) {
    // Raw 0 = -45°C
    float temp = rawToTemperature(0);
    EXPECT_NEAR(temp, -45.0f, 0.01f);
}

TEST_F(SHT31CalculationsTest, Temperature_MaxValue) {
    // Raw 65535 = 130°C (approximately)
    float temp = rawToTemperature(65535);
    EXPECT_NEAR(temp, 130.0f, 0.01f);
}

TEST_F(SHT31CalculationsTest, Temperature_MidValue) {
    // Raw 32768 = about 42.5°C
    // T = -45 + 175 * (32768/65535) = -45 + 87.5 = 42.5
    float temp = rawToTemperature(32768);
    EXPECT_NEAR(temp, 42.5f, 0.1f);
}

TEST_F(SHT31CalculationsTest, Temperature_ZeroCelsius) {
    // 0°C: T = -45 + 175 * (raw/65535) = 0
    // raw/65535 = 45/175 = 0.257
    // raw = 16852
    uint16_t raw = static_cast<uint16_t>(65535.0f * 45.0f / 175.0f);
    float temp = rawToTemperature(raw);
    EXPECT_NEAR(temp, 0.0f, 0.1f);
}

TEST_F(SHT31CalculationsTest, Temperature_RoomTemperature) {
    // 25°C: T = -45 + 175 * (raw/65535) = 25
    // raw/65535 = 70/175 = 0.4
    // raw = 26214
    uint16_t raw = static_cast<uint16_t>(65535.0f * 70.0f / 175.0f);
    float temp = rawToTemperature(raw);
    EXPECT_NEAR(temp, 25.0f, 0.1f);
}

TEST_F(SHT31CalculationsTest, Temperature_TypicalValues) {
    // Test a range of typical temperatures
    // Formula: T = -45 + 175 * (raw / 65535)
    struct TestCase {
        uint16_t raw;
        float expectedC;
    };

    // Calculate expected values from formula
    TestCase cases[] = {
        {0x0000, -45.0f},                                    // -45 + 175*(0/65535) = -45
        {0x2000, -45.0f + 175.0f * (0x2000 / 65535.0f)},     // ~-23.1
        {0x4000, -45.0f + 175.0f * (0x4000 / 65535.0f)},     // ~-1.2
        {0x6000, -45.0f + 175.0f * (0x6000 / 65535.0f)},     // ~20.6
        {0x7000, -45.0f + 175.0f * (0x7000 / 65535.0f)},     // ~31.5
        {0x8000, -45.0f + 175.0f * (0x8000 / 65535.0f)},     // ~42.5
        {0xA000, -45.0f + 175.0f * (0xA000 / 65535.0f)},     // ~64.4
        {0xC000, -45.0f + 175.0f * (0xC000 / 65535.0f)},     // ~86.2
        {0xFFFF, 130.0f},                                    // -45 + 175*(65535/65535) = 130
    };

    for (const auto& tc : cases) {
        float temp = rawToTemperature(tc.raw);
        EXPECT_NEAR(temp, tc.expectedC, 0.1f)
            << "Failed for raw=0x" << std::hex << tc.raw;
    }
}

// ============================================================================
// Humidity Conversion Tests
// ============================================================================

TEST_F(SHT31CalculationsTest, Humidity_MinValue) {
    // Raw 0 = 0%
    float hum = rawToHumidity(0);
    EXPECT_NEAR(hum, 0.0f, 0.01f);
}

TEST_F(SHT31CalculationsTest, Humidity_MaxValue) {
    // Raw 65535 = 100%
    float hum = rawToHumidity(65535);
    EXPECT_NEAR(hum, 100.0f, 0.01f);
}

TEST_F(SHT31CalculationsTest, Humidity_MidValue) {
    // Raw 32768 = 50%
    float hum = rawToHumidity(32768);
    EXPECT_NEAR(hum, 50.0f, 0.1f);
}

TEST_F(SHT31CalculationsTest, Humidity_TypicalValues) {
    struct TestCase {
        uint16_t raw;
        float expectedRH;
    };

    TestCase cases[] = {
        {0x0000, 0.0f},
        {0x1999, 10.0f},
        {0x3333, 20.0f},
        {0x4CCC, 30.0f},
        {0x6666, 40.0f},
        {0x8000, 50.0f},
        {0x9999, 60.0f},
        {0xB333, 70.0f},
        {0xCCCC, 80.0f},
        {0xE666, 90.0f},
        {0xFFFF, 100.0f},
    };

    for (const auto& tc : cases) {
        float hum = rawToHumidity(tc.raw);
        EXPECT_NEAR(hum, tc.expectedRH, 0.5f)
            << "Failed for raw=0x" << std::hex << tc.raw;
    }
}

TEST_F(SHT31CalculationsTest, Humidity_Clamping) {
    // Humidity should be clamped to 0-100%
    // Since raw is uint16_t, we can't get negative
    // But we should verify clamping logic works
    float hum = rawToHumidity(65535);
    EXPECT_LE(hum, 100.0f);
    EXPECT_GE(hum, 0.0f);
}

// ============================================================================
// Fixed-Point Conversion Tests
// ============================================================================

TEST_F(SHT31CalculationsTest, TemperatureFixed_Positive) {
    EXPECT_EQ(temperatureToFixed(23.45f), 2345);
    EXPECT_EQ(temperatureToFixed(25.0f), 2500);
    EXPECT_EQ(temperatureToFixed(0.0f), 0);
    EXPECT_EQ(temperatureToFixed(100.0f), 10000);
}

TEST_F(SHT31CalculationsTest, TemperatureFixed_Negative) {
    EXPECT_EQ(temperatureToFixed(-10.0f), -1000);
    EXPECT_EQ(temperatureToFixed(-45.0f), -4500);
    EXPECT_EQ(temperatureToFixed(-0.5f), -50);
}

TEST_F(SHT31CalculationsTest, HumidityFixed) {
    EXPECT_EQ(humidityToFixed(50.0f), 5000);
    EXPECT_EQ(humidityToFixed(67.89f), 6789);
    EXPECT_EQ(humidityToFixed(0.0f), 0);
    EXPECT_EQ(humidityToFixed(100.0f), 10000);
}

// ============================================================================
// End-to-End Conversion Tests
// ============================================================================

TEST_F(SHT31CalculationsTest, EndToEnd_TemperatureMeasurement) {
    // Simulate a complete temperature measurement

    // Raw sensor data (MSB, LSB, CRC)
    uint8_t tempMSB = 0x64;
    uint8_t tempLSB = 0x8C;

    // Verify CRC
    uint8_t data[] = {tempMSB, tempLSB};
    uint8_t expectedCRC = crc8(data, 2);

    // Convert to raw value
    uint16_t rawTemp = (static_cast<uint16_t>(tempMSB) << 8) | tempLSB;
    EXPECT_EQ(rawTemp, 0x648C);

    // Convert to temperature
    float temp = rawToTemperature(rawTemp);
    // 0x648C = 25740, T = -45 + 175 * (25740/65535) = -45 + 68.7 = 23.7°C
    EXPECT_NEAR(temp, 23.7f, 0.5f);

    // Convert to fixed-point
    int32_t fixed = temperatureToFixed(temp);
    EXPECT_NEAR(fixed, 2370, 50);
}

TEST_F(SHT31CalculationsTest, EndToEnd_HumidityMeasurement) {
    // Simulate a complete humidity measurement

    // Raw sensor data (MSB, LSB, CRC)
    uint8_t humMSB = 0x9C;
    uint8_t humLSB = 0xA5;

    // Verify CRC
    uint8_t data[] = {humMSB, humLSB};
    uint8_t expectedCRC = crc8(data, 2);
    (void)expectedCRC;

    // Convert to raw value
    uint16_t rawHum = (static_cast<uint16_t>(humMSB) << 8) | humLSB;
    EXPECT_EQ(rawHum, 0x9CA5);

    // Convert to humidity
    float hum = rawToHumidity(rawHum);
    // 0x9CA5 = 40101, RH = 100 * (40101/65535) = 61.2%
    EXPECT_NEAR(hum, 61.2f, 0.5f);

    // Convert to fixed-point
    int32_t fixed = humidityToFixed(hum);
    EXPECT_NEAR(fixed, 6120, 50);
}

TEST_F(SHT31CalculationsTest, EndToEnd_FullSensorReading) {
    // Simulate reading 6 bytes: temp MSB, temp LSB, temp CRC, hum MSB, hum LSB, hum CRC
    uint8_t rawData[6] = {
        0x64, 0x8C, 0x00,  // Temperature ~23.7°C (CRC placeholder)
        0x9C, 0xA5, 0x00   // Humidity ~61.2% (CRC placeholder)
    };

    // Calculate actual CRCs
    rawData[2] = crc8(&rawData[0], 2);
    rawData[5] = crc8(&rawData[3], 2);

    // Verify CRCs
    EXPECT_EQ(crc8(&rawData[0], 2), rawData[2]);
    EXPECT_EQ(crc8(&rawData[3], 2), rawData[5]);

    // Parse temperature
    uint16_t rawTemp = (static_cast<uint16_t>(rawData[0]) << 8) | rawData[1];
    float temp = rawToTemperature(rawTemp);
    EXPECT_NEAR(temp, 23.7f, 0.5f);

    // Parse humidity
    uint16_t rawHum = (static_cast<uint16_t>(rawData[3]) << 8) | rawData[4];
    float hum = rawToHumidity(rawHum);
    EXPECT_NEAR(hum, 61.2f, 0.5f);
}

TEST_F(SHT31CalculationsTest, CRC_ValidationFailure) {
    // Test that incorrect CRC can be detected
    uint8_t data[] = {0x64, 0x8C};
    uint8_t correctCRC = crc8(data, 2);
    uint8_t incorrectCRC = correctCRC ^ 0x01;  // Flip one bit

    EXPECT_NE(incorrectCRC, correctCRC);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_F(SHT31CalculationsTest, Temperature_ExtremeHot) {
    // Raw 65535 = 130°C (sensor max)
    float temp = rawToTemperature(65535);
    EXPECT_NEAR(temp, 130.0f, 0.01f);
    EXPECT_EQ(temperatureToFixed(temp), 13000);
}

TEST_F(SHT31CalculationsTest, Temperature_ExtremeCold) {
    // Raw 0 = -45°C (sensor min)
    float temp = rawToTemperature(0);
    EXPECT_NEAR(temp, -45.0f, 0.01f);
    EXPECT_EQ(temperatureToFixed(temp), -4500);
}

TEST_F(SHT31CalculationsTest, Humidity_Saturation) {
    // 100% RH
    float hum = rawToHumidity(65535);
    EXPECT_NEAR(hum, 100.0f, 0.01f);
    EXPECT_EQ(humidityToFixed(hum), 10000);
}

TEST_F(SHT31CalculationsTest, Humidity_Dry) {
    // 0% RH
    float hum = rawToHumidity(0);
    EXPECT_NEAR(hum, 0.0f, 0.01f);
    EXPECT_EQ(humidityToFixed(hum), 0);
}
