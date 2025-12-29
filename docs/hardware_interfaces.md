# STM32MP157F-DK2 Hardware Interface Mapping

This document maps the physical expansion headers on the STM32MP157F-DK2 Discovery Kit to their corresponding STM32MP157 peripheral functions. Information extracted from User Manual UM2637.

## Overview

The STM32MP157F-DK2 provides two main expansion interfaces:
- **Arduino Uno V3 connectors** (CN13, CN14, CN16, CN17) - Standard Arduino shield compatibility
- **40-pin GPIO expansion connector** (CN2) - Raspberry Pi shield compatibility

---

## I2C Buses

| Bus | Pins | Linux Device | Usage | Notes |
|-----|------|--------------|-------|-------|
| I2C1 | PD12 (SCL), PF15 (SDA) | /dev/i2c-0 | Shared | Audio codec, DSI touch, HDMI, USB hub |
| I2C4 | PZ4 (SCL), PZ5 (SDA) | /dev/i2c-2 | PMIC only | Reserved for STPMIC1 |
| I2C5 | PA11 (SCL), PA12 (SDA) | /dev/i2c-1 | Expansion | Arduino D14/D15, GPIO expansion pins 3/5 |

**Note:** I2C1 is shared between multiple on-board devices. For external sensors, prefer I2C5 on the expansion headers.

---

## SPI Buses

| Bus | Pins | Linux Device | Header |
|-----|------|--------------|--------|
| SPI4 | PE11 (NSS), PE12 (SCK), PE13 (MISO), PE14 (MOSI) | /dev/spidev0.0 | Arduino D10-D13 |
| SPI5 | PF6 (NSS), PF7 (SCK), PF8 (MISO), PF9 (MOSI) | /dev/spidev1.0 | GPIO expansion (pins 19-24) |

---

## UART/USART

| Interface | Pins | Linux Device | Usage |
|-----------|------|--------------|-------|
| UART4 | PG11 (TX), PB2 (RX) | /dev/ttySTM0 | ST-LINK serial console (115200 baud) |
| UART7 | PE8 (TX), PE7 (RX) | /dev/ttySTM1 | Arduino D0/D1 |
| USART2 | PD5 (TX), PD6 (RX), PD4 (RTS), PD3 (CTS) | - | Bluetooth module (on-board) |
| USART3 | PB10 (TX), PB12 (RX), PG8 (RTS), PB13 (CTS) | /dev/ttySTM2 | GPIO expansion (pins 8, 10, 11, 36) |

---

## ADC Channels

| Channel | Pin | Arduino | GPIO Exp | Notes |
|---------|-----|---------|----------|-------|
| ADC1_IN0 | ANA0 | A2 | - | Shared with ADC2_IN0 |
| ADC1_IN1 | ANA1 | A3 | - | Shared with ADC2_IN1 |
| ADC1_IN13 | PC3 | A4 | - | Default config (SB24 ON) |
| ADC1_IN6 | PF12 | A5 | - | Default config (SB26 ON) |
| ADC2_IN6 | PF14 | A0 | - | |
| ADC2_IN2 | PF13 | A1 | - | |

**Alternative A4/A5 config:** PA12 (I2C5_SDA) / PA11 (I2C5_SCL) via solder bridges SB23/SB25.

---

## Timer/PWM Outputs

| Timer | Channel | Pin | Header | Position |
|-------|---------|-----|--------|----------|
| TIM1 | CH1 | PE9 | Arduino | D6 |
| TIM1 | CH2 | PE11 | Arduino | D10 |
| TIM1 | CH4 | PE14 | Arduino | D11 |
| TIM3 | CH2 | PC7 | GPIO Exp | Pin 33 |
| TIM4 | CH2 | PD13 | GPIO Exp | Pin 32 |
| TIM4 | CH3 | PD14 | Arduino | D3 |
| TIM4 | CH4 | PD15 | Arduino | D5 |
| TIM5 | CH2 | PH11 | GPIO Exp | Pin 31 |
| TIM12 | CH1 | PH6 | Arduino | D9 |

---

## Arduino Connectors (CN13, CN14, CN16, CN17)

### CN16 - Power Header

| Pin | Name | Function |
|-----|------|----------|
| 1 | NC | Reserved for test |
| 2 | 3V3 | IOREF (3.3V reference) |
| 3 | NRST | Reset |
| 4 | 3V3 | 3.3V power |
| 5 | 5V | 5V power |
| 6 | GND | Ground |
| 7 | GND | Ground |
| 8 | VIN | Not connected |

### CN17 - Analog Header

| Pin | Name | STM32 Pin | Function |
|-----|------|-----------|----------|
| 1 | A0 | PF14 | ADC2_IN6 |
| 2 | A1 | PF13 | ADC2_IN2 |
| 3 | A2 | ANA0 | ADC1_IN0, ADC2_IN0 |
| 4 | A3 | ANA1 | ADC1_IN1, ADC2_IN1 |
| 5 | A4 | PC3 / PA12 | ADC1_IN13 (default) or I2C5_SDA |
| 6 | A5 | PF12 / PA11 | ADC1_IN6 (default) or I2C5_SCL |

### CN14 - Digital Header (D0-D7)

| Pin | Name | STM32 Pin | Function |
|-----|------|-----------|----------|
| 1 | D0 | PE7 | UART7_RX |
| 2 | D1 | PE8 | UART7_TX |
| 3 | D2 | PE1 | GPIO |
| 4 | D3 | PD14 | TIM4_CH3 (PWM) |
| 5 | D4 | PE10 | GPIO |
| 6 | D5 | PD15 | TIM4_CH4 (PWM) |
| 7 | D6 | PE9 | TIM1_CH1 (PWM) |
| 8 | D7 | PD1 | GPIO |

### CN13 - Digital Header (D8-D15)

| Pin | Name | STM32 Pin | Function |
|-----|------|-----------|----------|
| 1 | D8 | PG3 | GPIO |
| 2 | D9 | PH6 | TIM12_CH1 (PWM) |
| 3 | D10 | PE11 | SPI4_NSS, TIM1_CH2 |
| 4 | D11 | PE14 | SPI4_MOSI, TIM1_CH4 |
| 5 | D12 | PE13 | SPI4_MISO |
| 6 | D13 | PE12 | SPI4_SCK |
| 7 | GND | - | Ground |
| 8 | VREFP | - | VREF+ (ADC reference) |
| 9 | D14 | PA12 | I2C5_SDA |
| 10 | D15 | PA11 | I2C5_SCL |

---

## GPIO Expansion Connector CN2 (40-pin Raspberry Pi Compatible)

```
         3V3  (1)  (2)  5V
   I2C5_SDA  (3)  (4)  5V          PA12
   I2C5_SCL  (5)  (6)  GND         PA11
        MCO1  (7)  (8)  USART3_TX   PA8 / PB10
         GND  (9) (10)  USART3_RX         PB12
  USART3_RTS (11) (12)  SAI2_SCKA   PG8 / PI5
   SDMMC3_D3 (13) (14)  GND         PD7
   SDMMC3_CK (15) (16)  SDMMC3_CMD  PG15 / PF1
         3V3 (17) (18)  SDMMC3_D0         PF0
   SPI5_MOSI (19) (20)  GND         PF9
   SPI5_MISO (21) (22)  SDMMC3_D1   PF8 / PF4
    SPI5_SCK (23) (24)  SPI5_NSS    PF7 / PF6
         GND (25) (26)  GPIO7             PF3
    I2C1_SDA (27) (28)  I2C1_SCL    PF15 / PD12
        MCO2 (29) (30)  GND         PG2
    TIM5_CH2 (31) (32)  TIM4_CH2    PH11 / PD13
    TIM3_CH2 (33) (34)  GND         PC7
    SAI2_FSA (35) (36)  USART3_CTS  PI7 / PB13
   SDMMC3_D2 (37) (38)  SAI2_SDA    PF5 / PI6
         GND (39) (40)  SAI2_SDB          PF11
```

### Full CN2 Pinout Table

| Pin | STM32 | Function | Pin | STM32 | Function |
|-----|-------|----------|-----|-------|----------|
| 1 | - | 3V3 | 2 | - | 5V |
| 3 | PA12 | GPIO2 / I2C5_SDA | 4 | - | 5V |
| 5 | PA11 | GPIO3 / I2C5_SCL | 6 | - | GND |
| 7 | PA8 | GPIO4 / MCO1 | 8 | PB10 | GPIO14 / USART3_TX |
| 9 | - | GND | 10 | PB12 | GPIO15 / USART3_RX |
| 11 | PG8 | GPIO17 / USART3_RTS | 12 | PI5 | GPIO18 / SAI2_SCKA |
| 13 | PD7 | GPIO27 / SDMMC3_D3 | 14 | - | GND |
| 15 | PG15 | GPIO22 / SDMMC3_CK | 16 | PF1 | GPIO23 / SDMMC3_CMD |
| 17 | - | 3V3 | 18 | PF0 | GPIO24 / SDMMC3_D0 |
| 19 | PF9 | GPIO10 / SPI5_MOSI | 20 | - | GND |
| 21 | PF8 | GPIO9 / SPI5_MISO | 22 | PF4 | GPIO25 / SDMMC3_D1 |
| 23 | PF7 | GPIO11 / SPI5_SCK | 24 | PF6 | GPIO8 / SPI5_NSS |
| 25 | - | GND | 26 | PF3 | GPIO7 |
| 27 | PF15 | I2C1_SDA | 28 | PD12 | I2C1_SCL |
| 29 | PG2 | GPIO5 / MCO2 | 30 | - | GND |
| 31 | PH11 | GPIO6 / TIM5_CH2 | 32 | PD13 | GPIO12 / TIM4_CH2 |
| 33 | PC7 | GPIO13 / TIM3_CH2 | 34 | - | GND |
| 35 | PI7 | GPIO19 / SAI2_FSA | 36 | PB13 | GPIO16 / USART3_CTS |
| 37 | PF5 | GPIO26 / SDMMC3_D2 | 38 | PI6 | GPIO20 / SAI2_SDA |
| 39 | - | GND | 40 | PF11 | GPIO21 / SAI2_SDB |

---

## SDMMC Interfaces

| Interface | Pins | Usage |
|-----------|------|-------|
| SDMMC1 | PC8-PC12, PD2 | microSD card slot (CN15) |
| SDMMC2 | PB3, PB4, PB14, PB15, PG6, PE3 | WiFi module (on-board) |
| SDMMC3 | PF0-PF5, PD7, PG15 | GPIO expansion (pins 13-26) |

---

## On-Board Peripherals (Already Used)

These interfaces are used by on-board components and may have limited availability:

| Interface | Component | Sharing Notes |
|-----------|-----------|---------------|
| I2C1 | Audio codec, Touch panel, HDMI, USB Hub | Shared bus, available on CN2 pins 27/28 |
| I2C4 | STPMIC1 | Not available for external use |
| USART2 | Bluetooth module | Not available |
| SDMMC2 | WiFi module | Not available |
| SAI2 | Audio codec | Shared with CN2 pins 12, 35, 38, 40 (SB13-16 default ON) |

---

## Linux Device Mapping

| Device | Kernel Name | Function |
|--------|-------------|----------|
| /dev/i2c-0 | I2C1 | Shared bus (audio, touch, HDMI) |
| /dev/i2c-1 | I2C5 | Expansion headers |
| /dev/i2c-2 | I2C4 | PMIC (restricted) |
| /dev/spidev0.0 | SPI4 | Arduino header |
| /dev/spidev1.0 | SPI5 | GPIO expansion |
| /dev/ttySTM0 | UART4 | Console (ST-LINK) |
| /dev/ttySTM1 | UART7 | Arduino D0/D1 |
| /dev/ttyRPMSG0 | RPMsg | M4 coprocessor communication |

---

## Useful GPIO Numbers

For sysfs GPIO access, use `gpiochip` offsets:

| Port | Chip | Base | Example: PA8 |
|------|------|------|--------------|
| GPIOA | gpiochip0 | 0 | GPIO 8 |
| GPIOB | gpiochip1 | 16 | GPIO 16+pin |
| GPIOC | gpiochip2 | 32 | GPIO 32+pin |
| GPIOD | gpiochip3 | 48 | GPIO 48+pin |
| GPIOE | gpiochip4 | 64 | GPIO 64+pin |
| GPIOF | gpiochip5 | 80 | GPIO 80+pin |
| GPIOG | gpiochip6 | 96 | GPIO 96+pin |
| GPIOH | gpiochip7 | 112 | GPIO 112+pin |
| GPIOI | gpiochip8 | 128 | GPIO 128+pin |
| GPIOZ | gpiochip9 | 144 | GPIO 144+pin |

---

## Reference

- **User Manual:** UM2637 - Discovery kits with increased-frequency 800 MHz STM32MP157 MPUs
- **Datasheet:** STM32MP157 datasheet for full pin alternate function mapping
- **Errata:** ES0438 - STM32MP151x/3x/7x device errata
