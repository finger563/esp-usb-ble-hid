#pragma once

// credits to
// https://github.com/EasyConNS/BlueCon-esp32/blob/master/components/joycon/include/output_report.h

// output
#define OUTPUT_REPORT_SIZE 0x31 // 49 is OK?
#define OUTPUT_REPORT_CMD 0x01
#define OUTPUT_REPORT_RUM 0x10
#define OUTPUT_REPORT_MCU 0x11

namespace sp {

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

#pragma pack(1)
// 10~63(54)
typedef struct Subcmd {
  uint8_t subid; // x10, x21, x40, x48 etc.
  uint8_t raw[53];
} subcmd_t;

typedef struct Subcmd_10 {
  uint8_t subid; // x10
  uint32_t address;
  uint8_t length;
} subcmd_10_t;
#define SUBCMD_10 0x10
#define SUBCMD_10_INIT                                                                             \
  { SUBCMD_10, 0, 0, 0, 0, 0, 0 }

typedef struct Subcmd_21 {
  uint8_t subid; // x21
  mcu_cmd_t mcucmd : 8;
} subcmd_21_t;
#define SUBCMD_21 0x21

typedef struct Subcmd_30 {
  uint8_t cmd;        // 10
  uint8_t player : 4; // player_t
  uint8_t flash : 4;  // player_flash_t
} subcmd_30_t;

typedef struct Subcmd_40 {
  uint8_t subid; // x40
  uint8_t enable;
} subcmd_40_t;
#define SUBCMD_40 0x40
#define SUBCMD_40_INIT                                                                             \
  { SUBCMD_40, 0 }

typedef struct Subcmd_48 {
  uint8_t subid; // x48
  uint8_t enable_vibration;
} subcmd_48_t;
#define SUBCMD_48 0x48
#define SUBCMD_48_INIT                                                                             \
  { SUBCMD_48, 0 }

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

/* rcev data structure
uint8_t buf[0x40]; bzero(buf, 0x40);
buf[0] = 1; // 0x10 for rumble only
buf[1] = GlobalPacketNumber; // Increment by 1 for each packet sent. It loops in 0x0 - 0xF range.
memcpy(buf + 2, rumbleData, 8);
buf[10] = subcommandID;
memcpy(buf + 11, subcommandData, subcommandDataLen);
hid_write(handle, buf, 0x40);
*/
typedef struct OutputReport {
  uint8_t id;      // 0 : 0x01,0x10,0x11
  uint8_t timer;   // 1
  rumble_t rumble; // 2~9(8)
  union {          // 10~63(54)
    subcmd_t subcmd;
    // TODO
    // subcmd_02_t subcmd_02;
    // subcmd_03_t subcmd_03;
    // subcmd_04_t subcmd_04;
    // subcmd_08_t subcmd_08;
    subcmd_10_t subcmd_10;
    // subcmd_11_t subcmd_11;
    subcmd_21_t subcmd_21;
    // subcmd_22_t subcmd_22;
    subcmd_30_t subcmd_30;
    subcmd_40_t subcmd_40;
    subcmd_48_t subcmd_48;
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

} // namespace sp
