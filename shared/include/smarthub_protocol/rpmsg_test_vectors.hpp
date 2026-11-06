/**
 * Messages as bytes on the wire, for tests.
 *
 * The application's tests and the firmware's each check these, so that a change in how either
 * side builds or reads a message fails on that side, without the two having to run together.
 */
#pragma once

#include <cstdint>
#include <vector>

namespace smarthub::protocol::vectors {

// type, flags, seq (2), len (2), reserved (2), then the payload

/** CMD_PING, sequence 0 */
inline const std::vector<uint8_t> PING = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/** RSP_PONG, sequence 0 */
inline const std::vector<uint8_t> PONG = {0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/** CMD_SET_GPIO, sequence 1: port B, pin 7, high, mode 0 */
inline const std::vector<uint8_t> SET_GPIO_B7_HIGH = {
    0x30, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00,
    0x01, 0x07, 0x01, 0x00};

/** CMD_SET_INTERVAL, sequence 2: 1000 ms */
inline const std::vector<uint8_t> SET_INTERVAL_1000 = {
    0x11, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00,
    0xE8, 0x03};

/** EVT_SENSOR_UPDATE, sequence 0x0102: sensor 3, temperature, 21.50 (2150 / 100), at 5000 ms */
inline const std::vector<uint8_t> SENSOR_UPDATE_21_50 = {
    0xC0, 0x00, 0x02, 0x01, 0x10, 0x00, 0x00, 0x00,
    0x03, 0x01, 0x00, 0x00,
    0x66, 0x08, 0x00, 0x00,
    0x64, 0x00, 0x00, 0x00,
    0x88, 0x13, 0x00, 0x00};

} // namespace smarthub::protocol::vectors
