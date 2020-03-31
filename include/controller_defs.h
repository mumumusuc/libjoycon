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

#ifndef CONTROLLER_DEFS_H
#define CONTROLLER_DEFS_H

#include "device.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLASH_MEM_SIZE 0x80000 // 524288 bytes
#define FLASH_MEM_STEP 0x1d
#define assert_flash_mem_address(add) ((add) < FLASH_MEM_SIZE && (add) >= 0)
#define assert_flash_mem_length(len) ((len) <= FLASH_MEM_STEP && (len) >= 0)

typedef enum Battery {
    BATT_EMPTY = 0,    // 0000
    BATT_CHARGING = 1, // 0001
    BATT_CRITICAL = 2, // 0010
    BATT_LOW = 4,      // 0100
    BATT_MEDIUM = 6,   // 0110
    BATT_FULL = 8,     // 1000
} battery_t;

typedef enum Category {
    PRO_GRIP = 0,
    JOYCON_L = 1,
    JOYCON_R = 2,
    JOYCON = 3,
} category_t;

typedef enum Power {
    SELF = 0,
    SWITCH = 1,
} power_t;

typedef enum ButtonState {
    RELEASE = 0,
    PRESSED = 1,
} button_state_t;

typedef enum Player {
#define _PLAYER(n) ((0x1 << (n)) - 1)
    PLAYER_0 = _PLAYER(0),
    PLAYER_1 = _PLAYER(1),
    PLAYER_2 = _PLAYER(2),
    PLAYER_3 = _PLAYER(3),
    PLAYER_4 = _PLAYER(4),
} player_t;
#define Player_(n) player_t(_PLAYER(n))

typedef enum PlayerFlash {
#define _PLAYER_FLASH(n) ((0x1 << (n)) - 1)
    PLAYER_FLASH_0 = _PLAYER_FLASH(0),
    PLAYER_FLASH_1 = _PLAYER_FLASH(1),
    PLAYER_FLASH_2 = _PLAYER_FLASH(2),
    PLAYER_FLASH_3 = _PLAYER_FLASH(3),
    PLAYER_FLASH_4 = _PLAYER_FLASH(4),
} player_flash_t;
#define PlayerFlash_(n) player_flash_t(_PLAYER_FLASH(n))

typedef enum HciMode {
    HCI_DISCONNECT = 0x0,
    HCI_RECONNECT = 0x1,
    HCI_REPAIR = 0x2,
    HCI_REBOOT = 0x4,
} hci_mode_t;

typedef enum GyroSensitivity {
    GYRO_SENS_250DPS = 0x0,
    GYRO_SENS_500DPS = 0x1,
    GYRO_SENS_1000DPS = 0x2,
    GYRO_SENS_2000DPS = 0x3,
#define GYRO_SENS_DEFAULT GYRO_SENS_2000DPS
} gyro_sensitivity_t;

typedef enum AccSensitivity {
    ACC_SENS_8G = 0x0,
    ACC_SENS_4G = 0x1,
    ACC_SENS_2G = 0x2,
    ACC_SENS_16G = 0x3,
#define ACC_SENS_DEFAULT ACC_SENS_8G
} acc_sensitivity_t;

typedef enum GyroPerformance {
    GYRO_PERF_833HZ = 0x0,
    GYRO_PERF_208HZ = 0x1,
#define GYRO_PERF_DEFAULT GYRO_PERF_208HZ
} gyro_performance_t;

typedef enum AccBandwidth {
    ACC_BW_200HZ = 0x0,
    ACC_BW_100HZ = 0x1,
#define ACC_BW_DEFAULT ACC_BW_100HZ
} acc_bandwidth_t;

typedef enum PollType {
    POLL_NFC_IR_CAM = 0x0,
    POLL_NFC_IR_MCU = 0x1,
    POLL_NFC_IR_DATA = 0x2,
    POLL_IR_CAM = 0x3,
    POLL_STANDARD = 0x30,
    POLL_NFC_IR = 0x31,
    POLL_33 = 0x33, // ?
    POLL_35 = 0x35, // ?
    POLL_SIMPLE_HID = 0x3F,
} poll_type_t;

#pragma pack(1)
typedef struct Button {
    union {
        struct {
            // Right byte
            button_state_t Y : 1;
            button_state_t X : 1;
            button_state_t B : 1;
            button_state_t A : 1;
            button_state_t RSR : 1; // right Joy-Con only
            button_state_t RSL : 1; // right Joy-Con only
            button_state_t R : 1;
            button_state_t ZR : 1;
            // Shared byte
            button_state_t MINUS : 1;
            button_state_t PLUS : 1;
            button_state_t RS : 1;
            button_state_t LS : 1;
            button_state_t HOME : 1;
            button_state_t CAPTURE : 1;
            button_state_t NA : 1;
            button_state_t CAHRGING_GRIP : 1;
            // Left byte
            button_state_t DPAD_DOWN : 1;
            button_state_t DPAD_UP : 1;
            button_state_t DPAD_RIGHT : 1;
            button_state_t DPAD_LEFT : 1;
            button_state_t LSR : 1; // left Joy-Con only
            button_state_t LSL : 1; // left Joy-Con only
            button_state_t L : 1;
            button_state_t ZL : 1;
        };
        struct {
            uint8_t left;
            uint8_t shared;
            uint8_t right;
        };
    };
} button_t;
STATIC_ASSERT(sizeof(Button) == 3, "sizeof Button == 3");
static inline void button_merge(button_t *__restrict dist, const button_t *__restrict src) {
    dist->left |= src->left;
    dist->shared |= src->shared;
    dist->right |= src->right;
};

typedef struct Stick {
    union {
        struct {
            uint16_t X : 12;
            uint16_t Y : 12;
        };
        uint8_t raw[3];
    };
} stick_t;
STATIC_ASSERT(sizeof(Stick) == 3, "sizeof Stick == 3");
static inline void stick_merge(stick_t *__restrict dist, const stick_t *__restrict src) {
    dist->raw[0] |= src->raw[0];
    dist->raw[1] |= src->raw[1];
    dist->raw[2] |= src->raw[2];
}

typedef struct Accelerator {
    int16_t X;
    int16_t Y;
    int16_t Z;
} accelerator_t;
STATIC_ASSERT(sizeof(Accelerator) == 6, "sizeof Accelerator == 6");

typedef struct Gyroscope {
    int16_t X;
    int16_t Y;
    int16_t Z;
} gyroscope_t;
STATIC_ASSERT(sizeof(Gyroscope) == 6, "sizeof Gyroscope == 6");

typedef struct ControllerState {
    power_t power : 1;
    category_t category : 2;
    uint8_t _ : 1;
    battery_t battery : 4;
} controller_state_t;
STATIC_ASSERT(sizeof(ControllerState) == 1, "sizeof ControllerState == 1");

typedef struct ControllerData {
    button_t button;
    stick_t left_stick;
    stick_t right_stick;
    void *meta[0];
} controller_data_t;
STATIC_ASSERT(sizeof(ControllerData) == 9, "sizeof ControllerData == 9");
static inline void
controller_data_merge(controller_data_t *__restrict dist, const controller_data_t *__restrict src) {
    button_merge(&dist->button, &src->button);
    stick_merge(&dist->left_stick, &src->left_stick);
    stick_merge(&dist->right_stick, &src->right_stick);
};

typedef struct ControllerInfo {
    uint8_t firmware[2];
    category_t category : 8;
    uint8_t _;
    mac_address_t mac_address;
} controller_info_t;
STATIC_ASSERT(sizeof(ControllerInfo) == 10, "sizeof ControllerInfo == 10");

// see : https://switchbrew.org/wiki/Joy-Con
/*
Color Name	                    Body HEX	Button HEX
Gray / グレー	                #828282	    #0F0F0F
Neon Red / ネオンレッド	        #FF3C28	    #1E0A0A
Neon Blue / ネオンブルー	    #0AB9E6	    #001E1E
Neon Yellow / ネオンイエロー	#E6FF00	    #142800
Neon Green / ネオングリーン	    #1EDC00	    #002800
Neon Pink / ネオンピンク	    #FF3278	    #28001E
Red / レッド	                #E10F00	    #280A0A
Blue / ブルー	                #4655F5	    #00000A
Neon Purple / ネオンパープル	#B400E6	    #140014
Neon Orange / ネオンオレンジ	#FAA005	    #0F0A00
Pokemon Let's Go! Pikachu / ポケットモンスター Let's Go! ピカチュウ
                                #FFDC00	    #322800
Pokemon Let's Go! Eevee / ポケットモンスター Let's Go! イーブイ
                                #C88C32	    #281900
Nintendo Labo Creators Contest Edition
                                #D7AA73	    #1E1914
*/
typedef struct ControllerColor {
    uint8_t body_color[3];
    uint8_t button_color[3];
    uint8_t left_grip_color[3];
    uint8_t right_grip_color[3];
    uint8_t _;
} controller_color_t;
STATIC_ASSERT(sizeof(ControllerColor) == 13, "sizeof ControllerColor == 13");
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpointer-sign"
#define _int_to_color(value) {(uint8_t)(((value)>>16)&0xff),(uint8_t)(((value)>>8)&0xff),(uint8_t)((value)&0xff)}
#define make_controller_color(body,button,grip_l,grip_r)  {_int_to_color(body),_int_to_color(button),_int_to_color(grip_l),_int_to_color(grip_r),0xff}
#define controller_body_color(color)       // TODO
#define controller_button_color(color)     // TODO
#define controller_left_grip_color(color)  // TODO
#define controller_right_grip_color(color) // TODO
#pragma clang diagnostic pop

typedef struct Patterns {
    uint8_t raw[23];
} patterns_t;

#pragma pack()

typedef struct HomeLightPattern {
    uint8_t intensity;
    uint8_t duration;
    uint8_t transition;
} home_light_pattern_t;

#define home_light_pattern(patterns, len)                                 \
    ({                                                                    \
        patterns_t p = {};                                                \
        for (unsigned i = 0; i < len; i++) {                              \
            home_light_pattern_t pts = (patterns)[i];                     \
            unsigned pos = i / 2;                                         \
            unsigned res = i % 2;                                         \
            p.raw[3 * pos] |= pts.intensity << (4 * (1 - res));           \
            p.raw[3 * pos + res + 1] =                                    \
                (uint8_t)((pts.transition << 4) | (pts.duration & 0x0F)); \
        }                                                                 \
        p;                                                                \
    })

static const home_light_pattern_t double_blink_pattern[] = {
    {
        .intensity = 0xF,
        .duration = 0x0,
        .transition = 0xF,
    },
    {
        .intensity = 0x0,
        .duration = 0x0,
        .transition = 0xF,
    },
    {
        .intensity = 0xF,
        .duration = 0x0,
        .transition = 0xF,
    },
    {
        .intensity = 0x0,
        .duration = 0x0,
        .transition = 0xF,
    },
    {
        .intensity = 0x0,
        .duration = 0x0,
        .transition = 0xF,
    },
    {
        .intensity = 0x0,
        .duration = 0x0,
        .transition = 0xF,
    },
};

#ifdef __cplusplus
}
#endif

#endif