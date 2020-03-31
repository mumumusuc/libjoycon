#ifndef MCU_H
#define MCU_H

#include "device.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// TODO: move to mcu.h(?)
typedef enum McuCmd {
    MCU_CMD_SET_MODE = 0x21, // 0x21
    MCU_CMD_WRITE = 0x23,    // 0x23
} mcu_cmd_t;

typedef enum McuSubcmd {
    MCU_SET_IR_MODE = 0x1,
    MCU_SET_IR_REG = 0x4,
} mcu_subcmd_t;

typedef enum McuMode {
    MCU_MODE_STANDBY = 0x1, // 1: Standby
    MCU_MODE_NFC = 0x4,     // 4: NFC
    MCU_MODE_IR = 0x5,      // 5: IR
    MCU_MODE_INIT = 0x6,    // 6: Initializing/FW Update?
} mcu_mode_t;

typedef enum McuState {
    MCU_STATE_SUSPEND = 0,
    MCU_STATE_RESUME,
    MCU_STATE_UPDATE,
} mcu_state_t;

typedef enum McuRegAddress {
    // 0x0004 - LSB Buffer Update Time - Default 0x32
    MCU_REG_ADDR_UPDATE_TIME = 0x0004,
    // 0x0007 - Finalize config -
    // Without this, the register changes do not have any effect.
    MCU_REG_ADDR_FINALIZE = 0x0007,
#define FINALIZE_TRUE 1
#define FINALIZE_FALSE 0
    // 0x00e0(?) - External light filter
    // LS o bit0: Off/On, bit1: 0x/1x, bit2: ??, bit4,5: ??.
    MCU_REG_ADDR_EXT_LIGHT_FILTER = 0x000e,
    // 0x0010 - Set IR Leds groups state - Only 3 LSB usable
    MCU_REG_ADDR_LEDS_STATE = 0x0010,
    // 0x0011 - Leds 1/2 Intensity - Max 0x0F.
    MCU_REG_ADDR_LEDS_1_2_INT = 0x0011,
    // 0x0012 - Leds 3/4 Intensity - Max 0x10.
    MCU_REG_ADDR_LEDS_3_4_INT = 0x0012,
    // 0x002d - Flip image - 0: Normal, 1: Vertically, 2: Horizontally, 3: Both
    MCU_REG_ADDR_FLIP_IMG = 0x002d,
    // 0x002e - Set Resolution based on sensor binning and skipping
    MCU_REG_ADDR_RESOLUTION = 0x002e,
    // 0x012e - Set digital gain LSB 4 bits of the value - 0-0xff
    MCU_REG_ADDR_DIGI_GAIN_LSB = 0x012e,
    // 0x012f - Set digital gain MSB 4 bits of the value - 0-0x7
    MCU_REG_ADDR_DIGI_GAIN_MSB = 0x012f,
    // 0x0130 - Set Exposure time LSByte - (31200 * us /1000) & 0xFF
    // Max: 600us, Max encoded: 0x4920.
    MCU_REG_ADDR_EXP_TIME_LSB = 0x0130,
    // 0x0131 - Set Exposure time MSByte - ((31200 * us /1000) & 0xFF00) >> 8
    MCU_REG_ADDR_EXP_TIME_MSB = 0x0131,
    // 0x0132 - Enable Max exposure Time - 0: Manual exposure, 1: Max exposure
    MCU_REG_ADDR_EXP_TIME_MAX = 0x0132,
#define EXP_TIME_MAX_ENABLE 1
#define EXP_TIME_MAX_MANUAL 0
    // 0x0143 - ExLF/White pixel stats threshold - 200: Default
    MCU_REG_ADDR_EXLF_THR = 0x0143,
#define EXLF_THR_DEFAULT 0xc8
    // 0x0167 - Enable De-noise smoothing algorithms - 0: Disable, 1: Enable.
    MCU_REG_ADDR_DENOISE_ALG = 0x0167,
#define DENOISE_ALG_ON 1
#define DENOISE_ALG_OFF 0
    //  0x0168 - Edge smoothing threshold - Max 0xFF, Default 0x23
    MCU_REG_ADDR_DENOISE_EDGE = 0x0168,
#define DENOISE_EDGE_DEFAULT 0x23
    // 0x0169 - Color Interpolation threshold - Max 0xFF, Default 0x44
    MCU_REG_ADDR_DENOISE_COLOR = 0x0169,
#define DENOISE_COLOR_DEFAULT 0x44
} mcu_reg_address_t;

typedef struct McuReg {
    u16_be_t address;
    uint8_t value;
} mcu_reg_t;
#define set_reg(a, v) \
    (mcu_reg_t) { u16_be(a), (uint8_t)(v) }
#define REG_FINALIZE set_reg(MCU_REG_ADDR_FINALIZE, FINALIZE_TRUE)

typedef enum IrMode {
    // 0,1/5/10+: Unknown
    IR_MODE_NONE = 0x2,             // 2: No mode/Disable?
    IR_MODE_MONENT = 0x3,           // 3: Moment
    IR_MODE_DPD = 0x4,              // 4: Dpd (Wii-style pointing)
    IR_MODE_CLUSTERING = 0x6,       // 6: Clustering
    IR_MODE_IMG_TRANSFER = 0x7,     // 7: Image transfer
    IR_MODE_ANSIS_SILHOUETTE = 0x8, // 8: Hand analysis, Silhouette
    IR_MODE_ANSIS_IMAGE = 0x9,      // 9: Hand analysis, Image
    IR_MODE_ANSIS_BOTH = 0xa,       // 10: Hand analysis, Silhouette/Image
} ir_mode_t;

typedef struct IrConfigFixed {
    ir_mode_t mode;
// Full pixel array
#define IR_CONFIG_RESOLUTION_240P 0b00000000
// Sensor Binning [2 X 2]
#define IR_CONFIG_RESOLUTION_120P 0b01010000
// Sensor Binning [4 x 2] and Skipping [1 x 2]
#define IR_CONFIG_RESOLUTION_60P 0b01100100
// Sensor Binning [4 x 2] and Skipping [2 x 4]
#define IR_CONFIG_RESOLUTION_30P 0b01101001 // 0x69
    uint8_t resolution;
    // uint8_t hand_ansis_mode;
    // uint8_t hand_ansis_threshold;
    // meta
#define IR_CONFIG_FRAGMENTS_240P 0xff
#define IR_CONFIG_FRAGMENTS_120P 0x3f
#define IR_CONFIG_FRAGMENTS_60P 0x0f
#define IR_CONFIG_FRAGMENTS_30P 0x03
    uint8_t fragments;
#define IR_CONFIG_UPDATE_DEFAULT 0x32
#define IR_CONFIG_UPDATE_30P 0x2d
    uint8_t update_time;
    size_t width;
    size_t height;
    u16_be_t major;
    u16_be_t minor;
} ir_config_fixed_t;

typedef struct IrConfigLive {
// Exposure time (Shutter speed) is in us. Valid values are 0 to 600us or 0
// - 1/1666.66s
#define ir_exposure_us(us) u16_le((us)*31200 / 1000)
    u16_be_t exposure;
    // uint16_t exposure;
#define IR_CONFIG_LED_BRIGHT_DIM 0b00000000
#define IR_CONFIG_LED_BRIGHT 0b00100000
#define IR_CONFIG_LED_DIM 0b00010000
#define IR_CONFIG_LED_NONE 0b00110000
#define IR_CONFIG_LED_FLASH 0b00000001
#define IR_CONFIG_LED_STROBE 0b10000000
    uint8_t leds; // Leds to enable, Strobe/Flashlight modes
    struct {
        uint8_t bright; // bright led value(1,2)
        uint8_t dim;    // dim led value(3,4)
    } intensity;
#define IR_CONFIG_EX_FILTER_ON 0x03
#define IR_CONFIG_EX_FILTER_OFF 0x00
    // External Light filter (Dark-frame subtraction). Additionally, disable if
    // leds in flashlight mode.
    uint8_t ex_light_filter;
#define IR_CONFIG_DIGI_GAIN_OFF 0x1
    uint8_t digi_gain;
    // De-noise algorithms
    struct {
        bool enable;
        uint8_t edge;
        uint8_t color;
    } denoise;
#define IR_CONFIG_FLIP_NORM 0
#define IR_CONFIG_FLIP_VERT 1
#define IR_CONFIG_FLIP_HORI 2
#define IR_CONFIG_FLIP_BOTH FLIP_IMG_VERT | FLIP_IMG_HORI
    uint8_t flip;
} ir_config_live_t;

typedef struct IrConfig {
    ir_config_fixed_t fixed;
    ir_config_live_t live;
} ir_config_t;

static const ir_config_fixed_t ir_config_240p = {
    .mode = IR_MODE_IMG_TRANSFER,
    .resolution = IR_CONFIG_RESOLUTION_240P,
    .fragments = IR_CONFIG_FRAGMENTS_240P,
    .update_time = IR_CONFIG_UPDATE_DEFAULT,
    .width = 320,
    .height = 240,
    //.major = u16_le(0x0008), // Set required IR MCU FW v5.18. Major 0x0005.
    //.minor = u16_le(0x001b), // Set required IR MCU FW v5.18. Minor 0x0018.
    .major = u16_be(0x0005),
    .minor = u16_be(0x0018),
};

static const ir_config_fixed_t ir_config_120p = {
    .mode = IR_MODE_IMG_TRANSFER,
    .resolution = IR_CONFIG_RESOLUTION_120P,
    .fragments = IR_CONFIG_FRAGMENTS_120P,
    .update_time = IR_CONFIG_UPDATE_DEFAULT,
    .width = 160,
    .height = 120,
    .major = u16_be(0x0005),
    .minor = u16_be(0x0018),
};

static const ir_config_fixed_t ir_config_60p = {
    .mode = IR_MODE_IMG_TRANSFER,
    .resolution = IR_CONFIG_RESOLUTION_60P,
    .fragments = IR_CONFIG_FRAGMENTS_60P,
    .update_time = IR_CONFIG_UPDATE_DEFAULT,
    .width = 80,
    .height = 60,
    .major = u16_be(0x0005),
    .minor = u16_be(0x0018),
};

static const ir_config_fixed_t ir_config_30p = {
    .mode = IR_MODE_IMG_TRANSFER,
    .resolution = IR_CONFIG_RESOLUTION_30P,
    .fragments = IR_CONFIG_FRAGMENTS_30P,
    .update_time = IR_CONFIG_UPDATE_30P,
    .width = 40,
    .height = 30,
    .major = u16_be(0x0005),
    .minor = u16_be(0x0018),
};

static const ir_config_t ir_test_config = {
    .fixed = ir_config_30p,
    .live =
        {
            .exposure = ir_exposure_us(100),
            .leds = IR_CONFIG_LED_BRIGHT | IR_CONFIG_LED_STROBE,
            .intensity = {0x70, 0x70},
            .ex_light_filter = IR_CONFIG_EX_FILTER_OFF,
            .digi_gain = 1,
            .denoise = {0, 0x7f, 0x7f},
            .flip = IR_CONFIG_FLIP_NORM,
        },
};

// TODO: move to flash.h(?)

typedef enum FlashAddress {
    FLASH_ADDR_MAC_LE = 0x0015,
#define FLASH_ADDR_MAC_LEN 6
    FLASH_ADDR_HOST_MAC_BE_1 = 0x2004,
    FLASH_ADDR_HOST_MAC_BE_2 = 0x202a,
#define FLASH_ADDR_HOST_MAC_LEN 6
    FLASH_ADDR_LTK_LE_1 = 0x200a,
    FLASH_ADDR_LTK_LE_2 = 0x2030,
#define FLASH_ADDR_LTK_LEN 16
    FLASH_ADDR_SN = 0x6000,
#define FLASH_ADDR_SN_LEN 16
    FLASH_ADDR_DEVICE_TYPE = 0x6012,
#define FLASH_ADDR_DEVICE_TYPE_LEN 1
    FLASH_ADDR_IMU_CALIB = 0x6020,
#define FLASH_ADDR_IMU_CALIB_LEN 24
    FLASH_ADDR_STICK_L_CALIB = 0x6030,
    FLASH_ADDR_STICK_R_CALIB = 0x6046,
#define FLASH_ADDR_STICK_CALIB_LEN 9
    FLASH_ADDR_COLOR = 0x6050,
#define FLASH_ADDR_COLOR_LEN 13
    FLASH_ADDR_IMU_OFFSET_HORI = 0x6080,
#define FLASH_ADDR_IMU_OFFSET_LEN 6
} flash_address_t;

#ifdef __cplusplus
}
#endif

#endif