/*
 *   Copyright (c) 2020 mumumusuc

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.

 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef OUTPUT_REPORT_H
#define OUTPUT_REPORT_H

#include "controller_defs.h"
#include "mcu.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// output
#define OUTPUT_REPORT_SIZE 0x31 // 49 is OK?
#define OUTPUT_REPORT_CMD 0x01
#define OUTPUT_REPORT_RUM 0x10
#define OUTPUT_REPORT_PHL 0x11
#define OUTPUT_REPORT_USB 0x80

#pragma pack(1)

// 10~63(54)
typedef struct Subcmd {
    uint8_t cmd; // 10
    uint8_t raw[53];
} subcmd_t;

// Sub-command 0x01: Bluetooth manual pairing
typedef struct Subcmd_01 {
    uint8_t cmd;              // 10
    uint8_t subcmd;           // 11
    mac_address_le_t address; // 12~17(6), little endian
    uint8_t fixed[3];         // 18~20(3)
    alias_t alias;            // 21 ~40(20)
    uint8_t extra[8];         // 41~48(8)
} subcmd_01_t;
#define SUBCMD_01 0x01
#define SUBCMD_01_INIT                                         \
    {                                                          \
        SUBCMD_01, 0, MAC_ADDRESS_INIT, {0, 0x04, 0x3c}, {}, { \
            0x68, 0, 0xc0, 0x39, 0, 0, 0, 0                    \
        }                                                      \
    }

// Sub-command 0x02: Request device info
typedef struct Subcmd_02 {
    uint8_t cmd; // 10
} subcmd_02_t;
#define SUBCMD_02 0x02
#define SUBCMD_02_INIT \
    { SUBCMD_02 }

typedef struct Subcmd_03 {
    uint8_t cmd; // 10
    struct {
        union {
            poll_type_t poll_type : 8; // 11
            uint8_t raw[36];           // [11:46] 36
        };
        uint8_t crc;  // 47
        uint8_t tail; // 48
    };
} subcmd_03_t;
#define SUBCMD_03 0x03
#define SUBCMD_03_INIT \
    { SUBCMD_03, POLL_STANDARD }
#define calc_crc8_03(buff) \
    (buff)->subcmd_03.crc = crc8((buff)->subcmd_03.raw, 36)

typedef struct Subcmd_04 {
    uint8_t cmd; // 10
    u16_le_t time;
} subcmd_04_t;
#define SUBCMD_04 0x04
#define SUBCMD_04_INIT \
    { SUBCMD_04 }

typedef struct Subcmd_05 {
    uint8_t cmd; // 10
} subcmd_05_t;
#define SUBCMD_05 0x05
#define SUBCMD_05_INIT \
    { SUBCMD_05 }

typedef struct Subcmd_06 {
    uint8_t cmd; // 10
    hci_mode_t mode : 8;
} subcmd_06_t;
#define SUBCMD_06 0x06
#define SUBCMD_06_INIT \
    { SUBCMD_06 }

typedef struct Subcmd_07 {
    uint8_t cmd; // 10
} subcmd_07_t;
#define SUBCMD_07 0x07
#define SUBCMD_07_INIT \
    { SUBCMD_07 }

typedef struct Subcmd_08 {
    uint8_t cmd; // 10
    uint8_t enable;
} subcmd_08_t;
#define SUBCMD_08 0x08
#define SUBCMD_08_INIT \
    { SUBCMD_08, 0 }

typedef struct Subcmd_10 {
    uint8_t cmd; // 10
    uint32_t address;
    uint8_t length;
} subcmd_10_t;
#define SUBCMD_10 0x10
#define SUBCMD_10_INIT \
    { SUBCMD_10, 0, 0 }

typedef struct Subcmd_11 {
    uint8_t cmd; // 10
    uint32_t address;
    uint8_t length;
    uint8_t data[FLASH_MEM_STEP];
} subcmd_11_t;
#define SUBCMD_11 0x11
#define SUBCMD_11_INIT      \
    {                       \
        SUBCMD_11, 0, 0, {} \
    }

typedef struct Subcmd_12 {
    uint8_t cmd; // 10
    uint32_t address;
    uint8_t length;
} subcmd_12_t;
#define SUBCMD_12 0x12
#define SUBCMD_12_INIT \
    { SUBCMD_12, 0, 0 }

typedef struct Subcmd_20 {
    uint8_t cmd; // 10
} subcmd_20_t;
#define SUBCMD_20 0x20
#define SUBCMD_20_INIT \
    { SUBCMD_20 }

typedef struct Subcmd_21 {
    uint8_t cmd;          // 10
    mcu_cmd_t subcmd : 8; // 11
    union {
#define SUBCMD_21_CRC_LEN 36
        // crc8 begin [12:47] 36
        struct {
            uint8_t raw[SUBCMD_21_CRC_LEN]; // [12:47] 36
            uint8_t crc;                    // 48
        };
        struct {
            mcu_subcmd_t mcu_subcmd : 8; // 12
            union {
                // #21
                mcu_mode_t mcu_mode : 8; // 13
                // #23#01
                struct {
                    uint8_t mcu_ir_mode;   // 13
                    uint8_t mcu_fragments; // 14
                    u16_le_t mcu_major_v;  // [15:16] 2
                    u16_le_t mcu_minor_v;  // [17:18] 2
                };
                // #23#04
                struct {
                    uint8_t mcu_reg_size; // 13
                    mcu_reg_t reg[9];     // [14:40] 27
                };
            };
        };
    };
} subcmd_21_t;
#define SUBCMD_21 0x21
#define SUBCMD_21_INIT              \
    {                               \
        SUBCMD_21, mcu_cmd_t(0), {} \
    }
#define calc_crc8_21(buff) \
    (buff)->subcmd_21.crc = crc8(buff->subcmd_21.raw, SUBCMD_21_CRC_LEN)

typedef struct Subcmd_22 {
    uint8_t cmd; // 10
    mcu_state_t state;
} subcmd_22_t;
#define SUBCMD_22 0x22
#define SUBCMD_22_INIT \
    { SUBCMD_22 }

typedef struct Subcmd_30 {
    uint8_t cmd; // 10
    player_t player : 4;
    player_flash_t flash : 4;
} subcmd_30_t;
#define SUBCMD_30 0x30
#define SUBCMD_30_INIT \
    { SUBCMD_30, PLAYER_0, PLAYER_FLASH_0 }

typedef struct Subcmd_38 {
    uint8_t cmd;                 // 10
    uint8_t base_duration : 4;   // 0_L : 1~F = 8~175ms, 0 = OFF
    uint8_t pattern_count : 4;   // 0_H :
    uint8_t repeat_count : 4;    // 1_L : 0 = forever
    uint8_t start_intensity : 4; //
    patterns_t patterns;
} subcmd_38_t;
#define SUBCMD_38 0x38
#define SUBCMD_38_INIT            \
    {                             \
        SUBCMD_38, 0, 0, 0, 0, {} \
    }

typedef struct Subcmd_40 {
    uint8_t cmd; // 10
    uint8_t enable;
} subcmd_40_t;
#define SUBCMD_40 0x40
#define SUBCMD_40_INIT \
    { SUBCMD_40, 0 }

typedef struct Subcmd_41 {
    uint8_t cmd; // 10
    gyro_sensitivity_t gyro_sensitivity : 8;
    acc_sensitivity_t acc_sensitivity : 8;
    gyro_performance_t gyro_performance : 8;
    acc_bandwidth_t acc_bandwidth : 8;
} subcmd_41_t;
#define SUBCMD_41 0x41
#define SUBCMD_41_INIT                                                   \
    {                                                                    \
        SUBCMD_41, GYRO_SEN_DEFAULT, ACC_SEN_DEFAULT, GYRO_PERF_DEFAULT, \
            ACC_BW_DEFAULT                                               \
    }

typedef struct Subcmd_42 {
    uint8_t cmd; // 10
    uint8_t address;
    uint8_t operation;
    uint8_t value;
} subcmd_42_t;
#define SUBCMD_42 0x42
#define SUBCMD_42_INIT \
    { SUBCMD_42, 0, 0, 0 }

typedef struct Subcmd_43 {
    uint8_t cmd; // 10
    uint8_t address;
    uint8_t count;
} subcmd_43_t;
#define SUBCMD_43 0x43
#define SUBCMD_43_INIT \
    { SUBCMD_43, 0, 0 }

typedef struct Subcmd_48 {
    uint8_t cmd; // 10
    uint8_t enable_vibration;
} subcmd_48_t;
#define SUBCMD_48 0x48
#define SUBCMD_48_INIT \
    { SUBCMD_48, 0 }

typedef struct Subcmd_50 {
    uint8_t cmd; // 10
} subcmd_50_t;
#define SUBCMD_50 0x50
#define SUBCMD_50_INIT \
    { SUBCMD_50 }

typedef struct RumbleData {
    uint8_t freq_h;
    uint8_t freq_h_amp;
    uint8_t freq_l;
    uint8_t freq_l_amp;
} rumble_data_t;

typedef struct Rumble {
    union {
        uint8_t raw[8];
        struct {
            rumble_data_t rumble_l;
            rumble_data_t rumble_r;
        };
    };
} rumble_t;

typedef struct OutputReport {
    uint8_t id;    // 0 : 0x01,0x80,0x10,0x11
    uint8_t timer; // 1
    union {        // 2~63(62)
        uint8_t raw[62];
        uint8_t usb[0];
        struct {
            rumble_t rumble; // 2~9(8)
            union {          // 10~63(54)
                subcmd_t subcmd;
                subcmd_01_t subcmd_01;
                subcmd_03_t subcmd_03;
                subcmd_04_t subcmd_04;
                subcmd_08_t subcmd_08;
                subcmd_10_t subcmd_10;
                subcmd_11_t subcmd_11;
                subcmd_21_t subcmd_21;
                subcmd_22_t subcmd_22;
                subcmd_30_t subcmd_30;
                subcmd_38_t subcmd_38;
                subcmd_40_t subcmd_40;
                subcmd_43_t subcmd_43;
                subcmd_48_t subcmd_48;
            };
        };
    };
} output_report_t;

#pragma pack()

// this need not be packed
typedef struct RumbleDataF {
    float freq_h;
    float freq_h_amp;
    float freq_l;
    float freq_l_amp;
} rumble_data_f_t;

#ifdef __cplusplus
}
#endif

#endif