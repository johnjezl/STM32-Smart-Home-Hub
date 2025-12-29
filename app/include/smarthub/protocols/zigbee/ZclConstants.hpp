/**
 * Zigbee Cluster Library (ZCL) Constants
 *
 * Defines cluster IDs, attribute IDs, command IDs, and data types
 * used in ZCL communication with Zigbee devices.
 */
#pragma once

#include <cstdint>

namespace smarthub::zigbee::zcl {

/**
 * ZCL Frame Control bits
 */
namespace frame_ctrl {
    constexpr uint8_t CLUSTER_SPECIFIC  = 0x01;  // vs Global
    constexpr uint8_t MANUFACTURER_SPEC = 0x04;
    constexpr uint8_t DIRECTION_SERVER  = 0x08;  // Server to client
    constexpr uint8_t DISABLE_DEFAULT_RSP = 0x10;
}

/**
 * ZCL Global Commands (frame type = 0)
 */
namespace global_cmd {
    constexpr uint8_t READ_ATTRIBUTES              = 0x00;
    constexpr uint8_t READ_ATTRIBUTES_RSP          = 0x01;
    constexpr uint8_t WRITE_ATTRIBUTES             = 0x02;
    constexpr uint8_t WRITE_ATTRIBUTES_UNDIVIDED   = 0x03;
    constexpr uint8_t WRITE_ATTRIBUTES_RSP         = 0x04;
    constexpr uint8_t WRITE_ATTRIBUTES_NO_RSP      = 0x05;
    constexpr uint8_t CONFIGURE_REPORTING          = 0x06;
    constexpr uint8_t CONFIGURE_REPORTING_RSP      = 0x07;
    constexpr uint8_t READ_REPORTING_CONFIG        = 0x08;
    constexpr uint8_t READ_REPORTING_CONFIG_RSP    = 0x09;
    constexpr uint8_t REPORT_ATTRIBUTES            = 0x0A;
    constexpr uint8_t DEFAULT_RSP                  = 0x0B;
    constexpr uint8_t DISCOVER_ATTRIBUTES          = 0x0C;
    constexpr uint8_t DISCOVER_ATTRIBUTES_RSP      = 0x0D;
    constexpr uint8_t DISCOVER_COMMANDS_RECEIVED   = 0x11;
    constexpr uint8_t DISCOVER_COMMANDS_GENERATED  = 0x13;
}

/**
 * ZCL Status Codes
 */
namespace status {
    constexpr uint8_t SUCCESS                = 0x00;
    constexpr uint8_t FAILURE                = 0x01;
    constexpr uint8_t NOT_AUTHORIZED         = 0x7E;
    constexpr uint8_t MALFORMED_COMMAND      = 0x80;
    constexpr uint8_t UNSUP_CLUSTER_COMMAND  = 0x81;
    constexpr uint8_t UNSUP_GENERAL_COMMAND  = 0x82;
    constexpr uint8_t UNSUP_MANUF_CLUSTER_COMMAND = 0x83;
    constexpr uint8_t UNSUP_MANUF_GENERAL_COMMAND = 0x84;
    constexpr uint8_t INVALID_FIELD          = 0x85;
    constexpr uint8_t UNSUPPORTED_ATTRIBUTE  = 0x86;
    constexpr uint8_t INVALID_VALUE          = 0x87;
    constexpr uint8_t READ_ONLY              = 0x88;
    constexpr uint8_t INSUFFICIENT_SPACE     = 0x89;
    constexpr uint8_t DUPLICATE_EXISTS       = 0x8A;
    constexpr uint8_t NOT_FOUND              = 0x8B;
    constexpr uint8_t UNREPORTABLE_ATTRIBUTE = 0x8C;
    constexpr uint8_t INVALID_DATA_TYPE      = 0x8D;
    constexpr uint8_t INVALID_SELECTOR       = 0x8E;
    constexpr uint8_t WRITE_ONLY             = 0x8F;
    constexpr uint8_t INCONSISTENT_STARTUP_STATE = 0x90;
    constexpr uint8_t DEFINED_OUT_OF_BAND    = 0x91;
    constexpr uint8_t ACTION_DENIED          = 0x93;
    constexpr uint8_t TIMEOUT                = 0x94;
    constexpr uint8_t ABORT                  = 0x95;
    constexpr uint8_t INVALID_IMAGE          = 0x96;
    constexpr uint8_t WAIT_FOR_DATA          = 0x97;
    constexpr uint8_t NO_IMAGE_AVAILABLE     = 0x98;
    constexpr uint8_t REQUIRE_MORE_IMAGE     = 0x99;
    constexpr uint8_t NOTIFICATION_PENDING   = 0x9A;
    constexpr uint8_t HARDWARE_FAILURE       = 0xC0;
    constexpr uint8_t SOFTWARE_FAILURE       = 0xC1;
    constexpr uint8_t CALIBRATION_ERROR      = 0xC2;
    constexpr uint8_t UNSUPPORTED_CLUSTER    = 0xC3;
}

/**
 * ZCL Data Types
 */
namespace datatype {
    constexpr uint8_t NO_DATA       = 0x00;
    constexpr uint8_t DATA8         = 0x08;
    constexpr uint8_t DATA16        = 0x09;
    constexpr uint8_t DATA24        = 0x0A;
    constexpr uint8_t DATA32        = 0x0B;
    constexpr uint8_t DATA40        = 0x0C;
    constexpr uint8_t DATA48        = 0x0D;
    constexpr uint8_t DATA56        = 0x0E;
    constexpr uint8_t DATA64        = 0x0F;
    constexpr uint8_t BOOLEAN       = 0x10;
    constexpr uint8_t BITMAP8       = 0x18;
    constexpr uint8_t BITMAP16      = 0x19;
    constexpr uint8_t BITMAP24      = 0x1A;
    constexpr uint8_t BITMAP32      = 0x1B;
    constexpr uint8_t BITMAP40      = 0x1C;
    constexpr uint8_t BITMAP48      = 0x1D;
    constexpr uint8_t BITMAP56      = 0x1E;
    constexpr uint8_t BITMAP64      = 0x1F;
    constexpr uint8_t UINT8         = 0x20;
    constexpr uint8_t UINT16        = 0x21;
    constexpr uint8_t UINT24        = 0x22;
    constexpr uint8_t UINT32        = 0x23;
    constexpr uint8_t UINT40        = 0x24;
    constexpr uint8_t UINT48        = 0x25;
    constexpr uint8_t UINT56        = 0x26;
    constexpr uint8_t UINT64        = 0x27;
    constexpr uint8_t INT8          = 0x28;
    constexpr uint8_t INT16         = 0x29;
    constexpr uint8_t INT24         = 0x2A;
    constexpr uint8_t INT32         = 0x2B;
    constexpr uint8_t INT40         = 0x2C;
    constexpr uint8_t INT48         = 0x2D;
    constexpr uint8_t INT56         = 0x2E;
    constexpr uint8_t INT64         = 0x2F;
    constexpr uint8_t ENUM8         = 0x30;
    constexpr uint8_t ENUM16        = 0x31;
    constexpr uint8_t FLOAT16       = 0x38;
    constexpr uint8_t FLOAT32       = 0x39;
    constexpr uint8_t FLOAT64       = 0x3A;
    constexpr uint8_t OCTET_STR     = 0x41;
    constexpr uint8_t CHAR_STR      = 0x42;
    constexpr uint8_t LONG_OCTET_STR = 0x43;
    constexpr uint8_t LONG_CHAR_STR = 0x44;
    constexpr uint8_t ARRAY         = 0x48;
    constexpr uint8_t STRUCT        = 0x4C;
    constexpr uint8_t TOD           = 0xE0;  // Time of day
    constexpr uint8_t DATE          = 0xE1;
    constexpr uint8_t UTC           = 0xE2;
    constexpr uint8_t CLUSTER_ID    = 0xE8;
    constexpr uint8_t ATTR_ID       = 0xE9;
    constexpr uint8_t BAC_OID       = 0xEA;
    constexpr uint8_t IEEE_ADDR     = 0xF0;
    constexpr uint8_t SEC_KEY       = 0xF1;
    constexpr uint8_t UNKNOWN       = 0xFF;
}

/**
 * Cluster IDs - General
 */
namespace cluster {
    constexpr uint16_t BASIC                = 0x0000;
    constexpr uint16_t POWER_CONFIG         = 0x0001;
    constexpr uint16_t DEVICE_TEMP          = 0x0002;
    constexpr uint16_t IDENTIFY             = 0x0003;
    constexpr uint16_t GROUPS               = 0x0004;
    constexpr uint16_t SCENES               = 0x0005;
    constexpr uint16_t ON_OFF               = 0x0006;
    constexpr uint16_t ON_OFF_SWITCH_CONFIG = 0x0007;
    constexpr uint16_t LEVEL_CONTROL        = 0x0008;
    constexpr uint16_t ALARMS               = 0x0009;
    constexpr uint16_t TIME                 = 0x000A;
    constexpr uint16_t RSSI_LOCATION        = 0x000B;
    constexpr uint16_t ANALOG_INPUT         = 0x000C;
    constexpr uint16_t ANALOG_OUTPUT        = 0x000D;
    constexpr uint16_t ANALOG_VALUE         = 0x000E;
    constexpr uint16_t BINARY_INPUT         = 0x000F;
    constexpr uint16_t BINARY_OUTPUT        = 0x0010;
    constexpr uint16_t BINARY_VALUE         = 0x0011;
    constexpr uint16_t MULTISTATE_INPUT     = 0x0012;
    constexpr uint16_t MULTISTATE_OUTPUT    = 0x0013;
    constexpr uint16_t MULTISTATE_VALUE     = 0x0014;
    constexpr uint16_t OTA_UPGRADE          = 0x0019;
    constexpr uint16_t POLL_CONTROL         = 0x0020;
    constexpr uint16_t GREEN_POWER_PROXY    = 0x0021;

    // Closures
    constexpr uint16_t SHADE_CONFIG         = 0x0100;
    constexpr uint16_t DOOR_LOCK            = 0x0101;
    constexpr uint16_t WINDOW_COVERING      = 0x0102;

    // HVAC
    constexpr uint16_t PUMP_CONFIG          = 0x0200;
    constexpr uint16_t THERMOSTAT           = 0x0201;
    constexpr uint16_t FAN_CONTROL          = 0x0202;
    constexpr uint16_t THERMOSTAT_UI        = 0x0204;

    // Lighting
    constexpr uint16_t COLOR_CONTROL        = 0x0300;
    constexpr uint16_t BALLAST_CONFIG       = 0x0301;

    // Measurement & Sensing
    constexpr uint16_t ILLUMINANCE_MEASUREMENT    = 0x0400;
    constexpr uint16_t ILLUMINANCE_LEVEL_SENSING  = 0x0401;
    constexpr uint16_t TEMPERATURE_MEASUREMENT    = 0x0402;
    constexpr uint16_t PRESSURE_MEASUREMENT       = 0x0403;
    constexpr uint16_t FLOW_MEASUREMENT           = 0x0404;
    constexpr uint16_t RELATIVE_HUMIDITY          = 0x0405;
    constexpr uint16_t OCCUPANCY_SENSING          = 0x0406;

    // Security & Safety
    constexpr uint16_t IAS_ZONE             = 0x0500;
    constexpr uint16_t IAS_ACE              = 0x0501;
    constexpr uint16_t IAS_WD               = 0x0502;

    // Smart Energy
    constexpr uint16_t METERING             = 0x0702;

    // Electrical Measurement
    constexpr uint16_t ELECTRICAL_MEASUREMENT = 0x0B04;

    // Diagnostics
    constexpr uint16_t DIAGNOSTICS          = 0x0B05;

    // Touchlink
    constexpr uint16_t TOUCHLINK            = 0x1000;

    // Manufacturer-specific range: 0xFC00 - 0xFFFF
}

/**
 * Basic Cluster Attributes
 */
namespace attr::basic {
    constexpr uint16_t ZCL_VERSION        = 0x0000;
    constexpr uint16_t APP_VERSION        = 0x0001;
    constexpr uint16_t STACK_VERSION      = 0x0002;
    constexpr uint16_t HW_VERSION         = 0x0003;
    constexpr uint16_t MANUFACTURER_NAME  = 0x0004;
    constexpr uint16_t MODEL_ID           = 0x0005;
    constexpr uint16_t DATE_CODE          = 0x0006;
    constexpr uint16_t POWER_SOURCE       = 0x0007;
    constexpr uint16_t GENERIC_DEVICE_CLASS = 0x0008;
    constexpr uint16_t GENERIC_DEVICE_TYPE  = 0x0009;
    constexpr uint16_t PRODUCT_CODE       = 0x000A;
    constexpr uint16_t PRODUCT_URL        = 0x000B;
    constexpr uint16_t LOCATION_DESC      = 0x0010;
    constexpr uint16_t PHYSICAL_ENV       = 0x0011;
    constexpr uint16_t DEVICE_ENABLED     = 0x0012;
    constexpr uint16_t ALARM_MASK         = 0x0013;
    constexpr uint16_t DISABLE_LOCAL_CFG  = 0x0014;
    constexpr uint16_t SW_BUILD_ID        = 0x4000;
}

/**
 * Power Configuration Cluster Attributes
 */
namespace attr::power {
    constexpr uint16_t MAINS_VOLTAGE      = 0x0000;
    constexpr uint16_t MAINS_FREQUENCY    = 0x0001;
    constexpr uint16_t BATTERY_VOLTAGE    = 0x0020;
    constexpr uint16_t BATTERY_PERCENT    = 0x0021;
}

/**
 * On/Off Cluster Attributes
 */
namespace attr::onoff {
    constexpr uint16_t ON_OFF             = 0x0000;
    constexpr uint16_t GLOBAL_SCENE_CTRL  = 0x4000;
    constexpr uint16_t ON_TIME            = 0x4001;
    constexpr uint16_t OFF_WAIT_TIME      = 0x4002;
    constexpr uint16_t STARTUP_ON_OFF     = 0x4003;
}

/**
 * On/Off Cluster Commands
 */
namespace cmd::onoff {
    constexpr uint8_t OFF                 = 0x00;
    constexpr uint8_t ON                  = 0x01;
    constexpr uint8_t TOGGLE              = 0x02;
    constexpr uint8_t OFF_WITH_EFFECT     = 0x40;
    constexpr uint8_t ON_WITH_RECALL      = 0x41;
    constexpr uint8_t ON_WITH_TIMED_OFF   = 0x42;
}

/**
 * Level Control Cluster Attributes
 */
namespace attr::level {
    constexpr uint16_t CURRENT_LEVEL      = 0x0000;
    constexpr uint16_t REMAINING_TIME     = 0x0001;
    constexpr uint16_t MIN_LEVEL          = 0x0002;
    constexpr uint16_t MAX_LEVEL          = 0x0003;
    constexpr uint16_t ON_OFF_TRANSITION  = 0x0010;
    constexpr uint16_t ON_LEVEL           = 0x0011;
    constexpr uint16_t ON_TRANSITION_TIME = 0x0012;
    constexpr uint16_t OFF_TRANSITION_TIME = 0x0013;
    constexpr uint16_t DEFAULT_MOVE_RATE  = 0x0014;
    constexpr uint16_t OPTIONS            = 0x000F;
    constexpr uint16_t STARTUP_LEVEL      = 0x4000;
}

/**
 * Level Control Cluster Commands
 */
namespace cmd::level {
    constexpr uint8_t MOVE_TO_LEVEL       = 0x00;
    constexpr uint8_t MOVE                = 0x01;
    constexpr uint8_t STEP                = 0x02;
    constexpr uint8_t STOP                = 0x03;
    constexpr uint8_t MOVE_TO_LEVEL_ONOFF = 0x04;
    constexpr uint8_t MOVE_ONOFF          = 0x05;
    constexpr uint8_t STEP_ONOFF          = 0x06;
    constexpr uint8_t STOP_ONOFF          = 0x07;
    constexpr uint8_t MOVE_TO_CLOSEST_FREQ = 0x08;
}

/**
 * Color Control Cluster Attributes
 */
namespace attr::color {
    constexpr uint16_t CURRENT_HUE        = 0x0000;
    constexpr uint16_t CURRENT_SAT        = 0x0001;
    constexpr uint16_t REMAINING_TIME     = 0x0002;
    constexpr uint16_t CURRENT_X          = 0x0003;
    constexpr uint16_t CURRENT_Y          = 0x0004;
    constexpr uint16_t DRIFT_COMPENSATION = 0x0005;
    constexpr uint16_t COMPENSATION_TEXT  = 0x0006;
    constexpr uint16_t COLOR_TEMP         = 0x0007;
    constexpr uint16_t COLOR_MODE         = 0x0008;
    constexpr uint16_t OPTIONS            = 0x000F;
    constexpr uint16_t NUM_PRIMARIES      = 0x0010;
    constexpr uint16_t ENHANCED_CURRENT_HUE = 0x4000;
    constexpr uint16_t ENHANCED_COLOR_MODE  = 0x4001;
    constexpr uint16_t COLOR_LOOP_ACTIVE    = 0x4002;
    constexpr uint16_t COLOR_LOOP_DIRECTION = 0x4003;
    constexpr uint16_t COLOR_LOOP_TIME      = 0x4004;
    constexpr uint16_t COLOR_LOOP_START_HUE = 0x4005;
    constexpr uint16_t COLOR_LOOP_STORED_HUE = 0x4006;
    constexpr uint16_t COLOR_CAPABILITIES   = 0x400A;
    constexpr uint16_t COLOR_TEMP_MIN       = 0x400B;
    constexpr uint16_t COLOR_TEMP_MAX       = 0x400C;
    constexpr uint16_t STARTUP_COLOR_TEMP   = 0x4010;
}

/**
 * Color Control Cluster Commands
 */
namespace cmd::color {
    constexpr uint8_t MOVE_TO_HUE         = 0x00;
    constexpr uint8_t MOVE_HUE            = 0x01;
    constexpr uint8_t STEP_HUE            = 0x02;
    constexpr uint8_t MOVE_TO_SAT         = 0x03;
    constexpr uint8_t MOVE_SAT            = 0x04;
    constexpr uint8_t STEP_SAT            = 0x05;
    constexpr uint8_t MOVE_TO_HUE_SAT     = 0x06;
    constexpr uint8_t MOVE_TO_COLOR       = 0x07;
    constexpr uint8_t MOVE_COLOR          = 0x08;
    constexpr uint8_t STEP_COLOR          = 0x09;
    constexpr uint8_t MOVE_TO_COLOR_TEMP  = 0x0A;
    constexpr uint8_t ENH_MOVE_TO_HUE     = 0x40;
    constexpr uint8_t ENH_MOVE_HUE        = 0x41;
    constexpr uint8_t ENH_STEP_HUE        = 0x42;
    constexpr uint8_t ENH_MOVE_TO_HUE_SAT = 0x43;
    constexpr uint8_t COLOR_LOOP_SET      = 0x44;
    constexpr uint8_t STOP_MOVE_STEP      = 0x47;
    constexpr uint8_t MOVE_COLOR_TEMP     = 0x4B;
    constexpr uint8_t STEP_COLOR_TEMP     = 0x4C;
}

/**
 * Temperature Measurement Cluster Attributes
 */
namespace attr::temperature {
    constexpr uint16_t MEASURED_VALUE     = 0x0000;  // In 0.01°C
    constexpr uint16_t MIN_MEASURED       = 0x0001;
    constexpr uint16_t MAX_MEASURED       = 0x0002;
    constexpr uint16_t TOLERANCE          = 0x0003;
}

/**
 * Relative Humidity Measurement Cluster Attributes
 */
namespace attr::humidity {
    constexpr uint16_t MEASURED_VALUE     = 0x0000;  // In 0.01%
    constexpr uint16_t MIN_MEASURED       = 0x0001;
    constexpr uint16_t MAX_MEASURED       = 0x0002;
    constexpr uint16_t TOLERANCE          = 0x0003;
}

/**
 * Occupancy Sensing Cluster Attributes
 */
namespace attr::occupancy {
    constexpr uint16_t OCCUPANCY          = 0x0000;
    constexpr uint16_t SENSOR_TYPE        = 0x0001;
    constexpr uint16_t PIR_O_TO_U_DELAY   = 0x0010;
    constexpr uint16_t PIR_U_TO_O_DELAY   = 0x0011;
    constexpr uint16_t PIR_U_TO_O_THRESH  = 0x0012;
}

/**
 * IAS Zone Cluster Attributes
 */
namespace attr::ias_zone {
    constexpr uint16_t ZONE_STATE         = 0x0000;
    constexpr uint16_t ZONE_TYPE          = 0x0001;
    constexpr uint16_t ZONE_STATUS        = 0x0002;
    constexpr uint16_t IAS_CIE_ADDRESS    = 0x0010;
    constexpr uint16_t ZONE_ID            = 0x0011;
    constexpr uint16_t NUM_ZONE_SENS_LEVELS = 0x0012;
    constexpr uint16_t CURRENT_ZONE_SENS_LEVEL = 0x0013;
}

/**
 * IAS Zone Types
 */
namespace zone_type {
    constexpr uint16_t STANDARD_CIE       = 0x0000;
    constexpr uint16_t MOTION_SENSOR      = 0x000D;
    constexpr uint16_t CONTACT_SWITCH     = 0x0015;
    constexpr uint16_t FIRE_SENSOR        = 0x0028;
    constexpr uint16_t WATER_SENSOR       = 0x002A;
    constexpr uint16_t CO_SENSOR          = 0x002B;
    constexpr uint16_t PERSONAL_EMERGENCY = 0x002C;
    constexpr uint16_t VIBRATION_MOVEMENT = 0x002D;
    constexpr uint16_t REMOTE_CONTROL     = 0x010F;
    constexpr uint16_t KEY_FOB            = 0x0115;
    constexpr uint16_t KEYPAD             = 0x021D;
    constexpr uint16_t STANDARD_WARNING   = 0x0225;
    constexpr uint16_t GLASS_BREAK        = 0x0226;
    constexpr uint16_t SECURITY_REPEATER  = 0x0229;
}

/**
 * Electrical Measurement Cluster Attributes
 */
namespace attr::electrical {
    constexpr uint16_t MEASUREMENT_TYPE   = 0x0000;
    constexpr uint16_t AC_FREQUENCY       = 0x0300;
    constexpr uint16_t RMS_VOLTAGE        = 0x0505;
    constexpr uint16_t RMS_CURRENT        = 0x0508;
    constexpr uint16_t ACTIVE_POWER       = 0x050B;
    constexpr uint16_t REACTIVE_POWER     = 0x050E;
    constexpr uint16_t APPARENT_POWER     = 0x050F;
    constexpr uint16_t POWER_FACTOR       = 0x0510;
    constexpr uint16_t AC_VOLTAGE_MULT    = 0x0600;
    constexpr uint16_t AC_VOLTAGE_DIV     = 0x0601;
    constexpr uint16_t AC_CURRENT_MULT    = 0x0602;
    constexpr uint16_t AC_CURRENT_DIV     = 0x0603;
    constexpr uint16_t AC_POWER_MULT      = 0x0604;
    constexpr uint16_t AC_POWER_DIV       = 0x0605;
}

/**
 * Metering Cluster Attributes
 */
namespace attr::metering {
    constexpr uint16_t CURRENT_SUMMATION  = 0x0000;
    constexpr uint16_t INSTANTANEOUS_DEMAND = 0x0400;
    constexpr uint16_t MULTIPLIER         = 0x0301;
    constexpr uint16_t DIVISOR            = 0x0302;
    constexpr uint16_t SUMMATION_FORMATTING = 0x0303;
    constexpr uint16_t DEMAND_FORMATTING  = 0x0304;
    constexpr uint16_t UNIT_OF_MEASURE    = 0x0300;
}

/**
 * Get data type size in bytes
 * @param type ZCL data type
 * @return Size in bytes, 0 for variable-length types
 */
inline size_t getDataTypeSize(uint8_t type) {
    switch (type) {
        case datatype::NO_DATA:   return 0;
        case datatype::DATA8:
        case datatype::BOOLEAN:
        case datatype::BITMAP8:
        case datatype::UINT8:
        case datatype::INT8:
        case datatype::ENUM8:     return 1;
        case datatype::DATA16:
        case datatype::BITMAP16:
        case datatype::UINT16:
        case datatype::INT16:
        case datatype::ENUM16:
        case datatype::FLOAT16:
        case datatype::CLUSTER_ID:
        case datatype::ATTR_ID:   return 2;
        case datatype::DATA24:
        case datatype::BITMAP24:
        case datatype::UINT24:
        case datatype::INT24:     return 3;
        case datatype::DATA32:
        case datatype::BITMAP32:
        case datatype::UINT32:
        case datatype::INT32:
        case datatype::FLOAT32:
        case datatype::TOD:
        case datatype::DATE:
        case datatype::UTC:
        case datatype::BAC_OID:   return 4;
        case datatype::DATA40:
        case datatype::BITMAP40:
        case datatype::UINT40:
        case datatype::INT40:     return 5;
        case datatype::DATA48:
        case datatype::BITMAP48:
        case datatype::UINT48:
        case datatype::INT48:     return 6;
        case datatype::DATA56:
        case datatype::BITMAP56:
        case datatype::UINT56:
        case datatype::INT56:     return 7;
        case datatype::DATA64:
        case datatype::BITMAP64:
        case datatype::UINT64:
        case datatype::INT64:
        case datatype::FLOAT64:
        case datatype::IEEE_ADDR: return 8;
        case datatype::SEC_KEY:   return 16;
        // Variable-length types
        case datatype::OCTET_STR:
        case datatype::CHAR_STR:
        case datatype::LONG_OCTET_STR:
        case datatype::LONG_CHAR_STR:
        case datatype::ARRAY:
        case datatype::STRUCT:
        default:                  return 0;
    }
}

} // namespace smarthub::zigbee::zcl
