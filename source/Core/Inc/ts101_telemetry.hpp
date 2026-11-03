#ifndef TS101_TELEMETRY_HPP_
#define TS101_TELEMETRY_HPP_
/*
 * TS101: a block of live values at the fixed address 0x20000000, for an external reader over SWD (an ESP32) that has no symbol
 * table. The linker keeps the first 64 bytes of RAM for it (section .telemetry, see stm32f103.ld and the Makefile).
 *
 * Reading (the core keeps running, nothing is halted, no debug register is touched): read seq, read the block, read seq again;
 * the copy is valid only when both reads are equal and even. Little endian, 64 bytes, see scripts/ts101_dog/README.md.
 * Commands: write cmd and cmd_arg first, then cmd_seq + 1; the firmware sets cmd_ack to cmd_seq once it is handled.
 */
#include "configuration.h"
#ifdef TS101_TELEMETRY
#include "OperatingModes.h"
#include <stdint.h>

#define TS101_TELEMETRY_MAGIC   0x54315354u // "TS1T"
#define TS101_TELEMETRY_VERSION 1

enum class TelemetryState : uint8_t { Off = 0, Heating = 1, Ready = 2, Cooling = 3, Load = 4, Boost = 5, Sleep = 6, Locked = 7, Warning = 8 };
enum TelemetryWarn : uint8_t { TELEMETRY_WARN_SUPPLY = 1 << 0, TELEMETRY_WARN_HANDLE = 1 << 1, TELEMETRY_WARN_NO_TIP = 1 << 2 };
enum TelemetryCmd : uint8_t { TELEMETRY_CMD_NONE = 0, TELEMETRY_CMD_LOT = 1, TELEMETRY_CMD_STOP = 2, TELEMETRY_CMD_START = 3 };

struct __attribute__((packed)) Ts101Telemetry {
  uint32_t magic;          // 0x00 'TS1T'
  uint16_t version;        // 0x04 1
  uint16_t size;           // 0x06 64
  uint32_t seq;            // 0x08 odd while being written
  uint32_t uptime_ms;      // 0x0C
  int16_t  tip_c;          // 0x10
  int16_t  target_c;       // 0x12 currentTempTargetDegC
  int16_t  handle_cx10;    // 0x14
  uint16_t vin_mv;         // 0x16
  uint16_t power_dw;       // 0x18 0.1 W
  uint8_t  pwm_pct;        // 0x1A 0-100
  uint8_t  state;          // 0x1B TelemetryState
  uint8_t  lot;            // 0x1C 0-4, 255 own target
  uint8_t  view;           // 0x1D 0 normal, 1 sensors
  int8_t   tilt_deg;       // 0x1E
  uint8_t  warn;           // 0x1F TelemetryWarn bits
  uint32_t last_move_ms;   // 0x20
  uint32_t last_button_ms; // 0x24
  uint16_t pd_mv;          // 0x28 0 without PD
  uint16_t pd_max_w;       // 0x2A
  uint32_t tip_raw_uv;     // 0x2C
  uint8_t  cmd;            // 0x30 from the ESP32, 0 none
  uint8_t  cmd_arg;        // 0x31
  uint8_t  cmd_seq;        // 0x32 the ESP32 counts up with each new command
  uint8_t  cmd_ack;        // 0x33 set to cmd_seq once handled
  uint32_t fb_addr;        // 0x34 the 512 bytes of the picture (OLED framebuffer)
  uint8_t  fb_w;           // 0x38 128
  uint8_t  fb_h;           // 0x39 32
  uint8_t  fb_layout;      // 0x3A 0 = SSD1306 pages: a byte is 8 rows, the lowest bit on top; 128 bytes a page, 4 pages
  uint8_t  reserved;       // 0x3B 0
  uint32_t fb_seq;         // 0x3C one more each time a picture went to the OLED
};
static_assert(sizeof(Ts101Telemetry) == 64, "the telemetry block is 64 bytes");
// The offsets the README gives the reader
static_assert(__builtin_offsetof(Ts101Telemetry, seq) == 0x08 && __builtin_offsetof(Ts101Telemetry, tip_c) == 0x10, "telemetry layout");
static_assert(__builtin_offsetof(Ts101Telemetry, pwm_pct) == 0x1A && __builtin_offsetof(Ts101Telemetry, last_move_ms) == 0x20, "telemetry layout");
static_assert(__builtin_offsetof(Ts101Telemetry, pd_mv) == 0x28 && __builtin_offsetof(Ts101Telemetry, tip_raw_uv) == 0x2C, "telemetry layout");
static_assert(__builtin_offsetof(Ts101Telemetry, cmd) == 0x30 && __builtin_offsetof(Ts101Telemetry, cmd_ack) == 0x33, "telemetry layout");
static_assert(__builtin_offsetof(Ts101Telemetry, fb_addr) == 0x34 && __builtin_offsetof(Ts101Telemetry, fb_seq) == 0x3C, "telemetry layout");

extern volatile Ts101Telemetry ts101Telemetry;

void          telemetryInit();                                                  // in main, before the threads start
void          telemetryUpdate(int16_t tipC, int32_t x10Watts);                  // PID thread, every loop; writes at 8 Hz
void          telemetryFrameSent();                                             // OLED::refresh, after a picture went out
OperatingMode telemetryGui(OperatingMode current, OperatingMode next, guiContext *cxt); // GUI thread, once a frame
#endif
#endif
