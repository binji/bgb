#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <SDL/SDL.h>
#include <SDL/SDL_main.h>

#define SUCCESS(x) ((x) == OK)
#define FAIL(x) ((x) != OK)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define ZERO_MEMORY(x) memset(&(x), 0, sizeof(x))

#define LOG(...) fprintf(stdout, __VA_ARGS__)
#define DEBUG(...)
#define PRINT_ERROR(...) fprintf(stderr, __VA_ARGS__)
#define CHECK_MSG(x, ...)                       \
  if (!(x)) {                                   \
    PRINT_ERROR("%s:%d: ", __FILE__, __LINE__); \
    PRINT_ERROR(__VA_ARGS__);                   \
    goto error;                                 \
  }
#define CHECK(x) \
  if (!(x)) {    \
    goto error;  \
  }

#define UNREACHABLE(...)      \
  do {                        \
    PRINT_ERROR(__VA_ARGS__); \
    exit(1);                  \
  } while (0)

#define NOT_IMPLEMENTED_NO_EMULATOR(...)  \
  do {                                    \
    printf("%s:%d:", __FILE__, __LINE__); \
    UNREACHABLE(__VA_ARGS__);             \
  } while (0)

#define NOT_IMPLEMENTED(...)  \
  do {                        \
    s_trace = TRUE;           \
    printf("\n\n");           \
    print_emulator_info(e);   \
    UNREACHABLE(__VA_ARGS__); \
  } while (0)

typedef uint16_t Address;
typedef uint16_t MaskedAddress;
typedef uint32_t RGBA;

#define RGBA_WHITE 0xffffffffu
#define RGBA_LIGHT_GRAY 0xffaaaaaau
#define RGBA_DARK_GRAY 0xff555555u
#define RGBA_BLACK 0xff000000u

#define ROM_U8(type, addr) ((type)*(rom_data->data + addr))
#define ROM_U16_BE(addr) \
  ((uint16_t)((ROM_U8(uint16_t, addr) << 8) | ROM_U8(uint16_t, addr + 1)))

#define ENTRY_POINT_START_ADDR 0x100
#define ENTRY_POINT_END_ADDR 0x103
#define LOGO_START_ADDR 0x104
#define LOGO_END_ADDR 0x133
#define TITLE_START_ADDR 0x134
#define TITLE_END_ADDR 0x143
#define MANUFACTURERS_CODE_START_ADDR 0x13F
#define MANUFACTURERS_CODE_END_ADDR 0x142
#define CGB_FLAG_ADDR 0x143
#define NEW_LINCENSEE_CODE_START_ADDR 0x144
#define NEW_LINCENSEE_CODE_END_ADDR 0x145
#define SGB_FLAG_ADDR 0x146
#define CARTRIDGE_TYPE_ADDR 0x147
#define ROM_SIZE_ADDR 0x148
#define RAM_SIZE_ADDR 0x149
#define DESTINATATION_CODE_ADDR 0x14a
#define OLD_LICENSEE_CODE_ADDR 0x14b
#define MASK_ROM_VERSION_NUMBER_ADDR 0x14c
#define HEADER_CHECKSUM_ADDR 0x14d
#define GLOBAL_CHECKSUM_START_ADDR 0x14e
#define GLOBAL_CHECKSUM_END_ADDR 0x14f

#define NEW_LICENSEE_CODE 0x33
#define HEADER_CHECKSUM_RANGE_START 0x134
#define HEADER_CHECKSUM_RANGE_END 0x14c

#define MINIMUM_ROM_SIZE 32768
#define ROM_BANK_SHIFT 14
#define ROM_BANK_BYTE_SIZE (1 << ROM_BANK_SHIFT)
#define VIDEO_RAM_SIZE 8192
#define WORK_RAM_SIZE 32768
#define HIGH_RAM_SIZE 127

#define RENDER_WIDTH (SCREEN_WIDTH * RENDER_SCALE)
#define RENDER_HEIGHT (SCREEN_HEIGHT * RENDER_SCALE)
#define RENDER_SCALE 4
#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 144
#define TILE_COUNT (256 + 256) /* Actually 256+128, but we mirror the middle. */
#define TILE_WIDTH 8
#define TILE_HEIGHT 8
#define MAP_COUNT 2
#define TILE_MAP_WIDTH 32
#define TILE_MAP_HEIGHT 32

#define WINDOW_MAX_X 166
#define WINDOW_MAX_Y 143
#define WINDOW_X_OFFSET 7

#define OBJ_COUNT 40
#define OBJ_PER_LINE_COUNT 10
#define OBJ_PALETTE_COUNT 2
#define OBJ_Y_OFFSET 16
#define OBJ_X_OFFSET 8

#define PALETTE_COLOR_COUNT 4

#define CHANNEL_COUNT 4
#define CHANNEL1 0
#define CHANNEL2 1
#define CHANNEL3 2
#define CHANNEL4 3

#define SOUND_COUNT 5
#define SOUND1 0
#define SOUND2 1
#define SOUND3 2
#define SOUND4 3
#define VIN 4

#define GB_CYCLES_PER_SECOND 4194304
#define DIV_CYCLES 256
#define TIMA_4096_CYCLES 1024
#define TIMA_262144_CYCLES 16
#define TIMA_65536_CYCLES 64
#define TIMA_16384_CYCLES 256
#define FRAME_CYCLES 70224
#define LINE_CYCLES 456
#define HBLANK_CYCLES 204         /* LCD STAT mode 0 */
#define VBLANK_CYCLES 4560        /* LCD STAT mode 1 */
#define USING_OAM_CYCLES 80       /* LCD STAT mode 2 */
#define USING_OAM_VRAM_CYCLES 172 /* LCD STAT mode 3 */
#define DMA_CYCLES 640

#define ADDR_MASK_4K 0x0fff
#define ADDR_MASK_8K 0x1fff
#define ADDR_MASK_16K 0x3fff
#define ADDR_MASK_32K 0x7fff

#define MBC1_RAM_ENABLED_MASK 0xf
#define MBC1_RAM_ENABLED_VALUE 0xa
#define MBC1_ROM_BANK_LO_MASK 0x1f
#define MBC1_BANK_HI_MASK 0x3
#define MBC1_BANK_HI_SHIFT 5

#define OAM_START_ADDR 0xfe00
#define OAM_END_ADDR 0xfe9f
#define UNUSED_START_ADDR 0xfea0
#define UNUSED_END_ADDR 0xfeff
#define IO_START_ADDR 0xff00
#define IO_END_ADDR 0xff7f
#define HIGH_RAM_START_ADDR 0xff80
#define HIGH_RAM_END_ADDR 0xfffe

#define OAM_TRANSFER_SIZE (OAM_END_ADDR - OAM_START_ADDR + 1)

#define FOREACH_IO_REG(V)                                    \
  V(JOYP, 0x00) /* Joypad */                                 \
  V(SB, 0x01)   /* Serial transfer data */                   \
  V(SC, 0x02)   /* Serial transfer control */                \
  V(DIV, 0x04)  /* Divider */                                \
  V(TIMA, 0x05) /* Timer counter */                          \
  V(TMA, 0x06)  /* Timer modulo */                           \
  V(TAC, 0x07)  /* Timer control */                          \
  V(IF, 0x0f)   /* Interrupt request */                      \
  V(NR10, 0x10) /* Channel 1 sweep */                        \
  V(NR11, 0x11) /* Channel 1 sound length/wave pattern */    \
  V(NR12, 0x12) /* Channel 1 volume envelope */              \
  V(NR13, 0x13) /* Channel 1 frequency lo */                 \
  V(NR14, 0x14) /* Channel 1 frequency hi */                 \
  V(NR21, 0x16) /* Channel 2 sound length/wave pattern */    \
  V(NR22, 0x17) /* Channel 2 volume envelope */              \
  V(NR23, 0x18) /* Channel 2 frequency lo */                 \
  V(NR24, 0x19) /* Channel 2 frequency hi */                 \
  V(NR30, 0x1a) /* Channel 3 sound on/off */                 \
  V(NR31, 0x1b) /* Channel 3 sound length */                 \
  V(NR32, 0x1c) /* Channel 3 select output level */          \
  V(NR33, 0x1d) /* Channel 3 frequency lo */                 \
  V(NR34, 0x1e) /* Channel 3 frequency hi */                 \
  V(NR41, 0x20) /* Channel 4 sound length */                 \
  V(NR42, 0x21) /* Channel 4 volume envelope */              \
  V(NR43, 0x22) /* Channel 4 polynomial counter */           \
  V(NR44, 0x23) /* Channel 4 counter/consecutive; initial */ \
  V(NR50, 0x24) /* Sound volume */                           \
  V(NR51, 0x25) /* Sound output select */                    \
  V(NR52, 0x26) /* Sound enabled */                          \
  V(LCDC, 0x40) /* LCD control */                            \
  V(STAT, 0x41) /* LCD status */                             \
  V(SCY, 0x42)  /* Screen Y */                               \
  V(SCX, 0x43)  /* Screen X */                               \
  V(LY, 0x44)   /* Y Line */                                 \
  V(LYC, 0x45)  /* Y Line compare */                         \
  V(DMA, 0x46)  /* DMA transfer to OAM */                    \
  V(BGP, 0x47)  /* BG palette */                             \
  V(OBP0, 0x48) /* OBJ palette 0 */                          \
  V(OBP1, 0x49) /* OBJ palette 1 */                          \
  V(WY, 0x4a)   /* Window Y */                               \
  V(WX, 0x4b)   /* Window X */                               \
  V(IE, 0xff)   /* Interrupt enable */

#define INTERRUPT_VBLANK_MASK 0x01
#define INTERRUPT_LCD_STAT_MASK 0x02
#define INTERRUPT_TIMER_MASK 0x04
#define INTERRUPT_SERIAL_MASK 0x08
#define INTERRUPT_JOYPAD_MASK 0x10

#define FROM_REG(X, MACRO) MACRO(X, DECODE)
#define TO_REG(X, MACRO) MACRO(X, ENCODE)
#define BITS_MASK(HI, LO) ((1 << ((HI) - (LO) + 1)) - 1)
#define ENCODE(X, HI, LO) (((X) & BITS_MASK(HI, LO)) << (LO))
#define DECODE(X, HI, LO) (((X) >> (LO)) & BITS_MASK(HI, LO))
#define BITS(X, OP, HI, LO) OP(X, HI, LO)
#define BIT(X, OP, B) OP(X, B, B)

#define CPU_FLAG_Z(X, OP) BIT(X, OP, 7)
#define CPU_FLAG_N(X, OP) BIT(X, OP, 6)
#define CPU_FLAG_H(X, OP) BIT(X, OP, 5)
#define CPU_FLAG_C(X, OP) BIT(X, OP, 4)

#define JOYP_JOYPAD_SELECT(X, OP) BITS(X, OP, 5, 4)
#define JOYP_DPAD_DOWN(X, OP) BIT(X, OP, 3)
#define JOYP_DPAD_UP(X, OP) BIT(X, OP, 2)
#define JOYP_DPAD_LEFT(X, OP) BIT(X, OP, 1)
#define JOYP_DPAD_RIGHT(X, OP) BIT(X, OP, 0)
#define JOYP_BUTTON_START(X, OP) BIT(X, OP, 3)
#define JOYP_BUTTON_SELECT(X, OP) BIT(X, OP, 2)
#define JOYP_BUTTON_B(X, OP) BIT(X, OP, 1)
#define JOYP_BUTTON_A(X, OP) BIT(X, OP, 0)

#define TAC_TIMER_ON(X, OP) BIT(X, OP, 2)
#define TAC_INPUT_CLOCK_SELECT(X, OP) BITS(X, OP, 1, 0)

/* TODO better names for all sound stuff */
#define NR10_SWEEP_TIME(X, OP) BITS(X, OP, 6, 4)
#define NR10_SWEEP_DIRECTION(X, OP) BIT(X, OP, 3)
#define NR10_SWEEP_COUNT(X, OP) BITS(X, OP, 2, 0)

#define NRX1_WAVE_DUTY(X, OP) BITS(X, OP, 7, 6)
#define NRX1_SOUND_LENGTH(X, OP) BITS(X, OP, 5, 0)

#define NRX2_INITIAL_VOLUME(X, OP) BITS(X, OP, 7, 4)
#define NRX2_ENVELOPE_DIRECTION(X, OP) BIT(X, OP, 3)
#define NRX2_ENVELOPE_COUNT(X, OP) BITS(X, OP, 2, 0)

#define NRX4_INITIAL(X, OP) BIT(X, OP, 7)
#define NRX4_COUNTER_CONSECUTIVE(X, OP) BIT(X, OP, 6)
#define NRX4_FREQUENCY_HI(X, OP) BITS(X, OP, 2, 0)

#define NR30_SOUND_ON(X, OP) BIT(X, OP, 7)

#define NR32_SELECT_OUTPUT_LEVEL(X, OP) BITS(X, OP, 6, 5)

#define NR43_SHIFT_CLOCK_FREQUENCY(X, OP) BITS(X, OP, 7, 4)
#define NR43_COUNTER_STEP(X, OP) BIT(X, OP, 3)
#define NR43_DIVIDE_RATIO(X, OP) BITS(X, OP, 2, 0)

#define OBJ_PRIORITY(X, OP) BIT(X, OP, 7)
#define OBJ_YFLIP(X, OP) BIT(X, OP, 6)
#define OBJ_XFLIP(X, OP) BIT(X, OP, 5)
#define OBJ_PALETTE(X, OP) BIT(X, OP, 4)

#define SC_TRANSFER_START(X, OP) BIT(X, OP, 7)
#define SC_CLOCK_SPEED(X, OP) BIT(X, OP, 1)
#define SC_SHIFT_CLOCK(X, OP) BIT(X, OP, 0)

#define NR50_VIN_SO2(X, OP) BIT(X, OP, 7)
#define NR50_SO2_VOLUME(X, OP) BITS(X, OP, 6, 4)
#define NR50_VIN_SO1(X, OP) BIT(X, OP, 3)
#define NR50_SO1_VOLUME(X, OP) BITS(X, OP, 2, 0)

#define NR51_SOUND4_SO2(X, OP) BIT(X, OP, 7)
#define NR51_SOUND3_SO2(X, OP) BIT(X, OP, 6)
#define NR51_SOUND2_SO2(X, OP) BIT(X, OP, 5)
#define NR51_SOUND1_SO2(X, OP) BIT(X, OP, 4)
#define NR51_SOUND4_SO1(X, OP) BIT(X, OP, 3)
#define NR51_SOUND3_SO1(X, OP) BIT(X, OP, 2)
#define NR51_SOUND2_SO1(X, OP) BIT(X, OP, 1)
#define NR51_SOUND1_SO1(X, OP) BIT(X, OP, 0)

#define NR52_ALL_SOUND_ENABLED(X, OP) BIT(X, OP, 7)
#define NR52_SOUND4_ON(X, OP) BIT(X, OP, 3)
#define NR52_SOUND3_ON(X, OP) BIT(X, OP, 2)
#define NR52_SOUND2_ON(X, OP) BIT(X, OP, 1)
#define NR52_SOUND1_ON(X, OP) BIT(X, OP, 0)

#define LCDC_DISPLAY(X, OP) BIT(X, OP, 7)
#define LCDC_WINDOW_TILE_MAP_SELECT(X, OP) BIT(X, OP, 6)
#define LCDC_WINDOW_DISPLAY(X, OP) BIT(X, OP, 5)
#define LCDC_BG_TILE_DATA_SELECT(X, OP) BIT(X, OP, 4)
#define LCDC_BG_TILE_MAP_SELECT(X, OP) BIT(X, OP, 3)
#define LCDC_OBJ_SIZE(X, OP) BIT(X, OP, 2)
#define LCDC_OBJ_DISPLAY(X, OP) BIT(X, OP, 1)
#define LCDC_BG_DISPLAY(X, OP) BIT(X, OP, 0)

#define STAT_YCOMPARE_INTR(X, OP) BIT(X, OP, 6)
#define STAT_USING_OAM_INTR(X, OP) BIT(X, OP, 5)
#define STAT_VBLANK_INTR(X, OP) BIT(X, OP, 4)
#define STAT_HBLANK_INTR(X, OP) BIT(X, OP, 3)
#define STAT_YCOMPARE(X, OP) BIT(X, OP, 2)
#define STAT_MODE(X, OP) BITS(X, OP, 1, 0)

#define PALETTE_COLOR3(X, OP) BITS(X, OP, 7, 6)
#define PALETTE_COLOR2(X, OP) BITS(X, OP, 5, 4)
#define PALETTE_COLOR1(X, OP) BITS(X, OP, 3, 2)
#define PALETTE_COLOR0(X, OP) BITS(X, OP, 1, 0)

#define FOREACH_RESULT(V) \
  V(OK, 0)                \
  V(ERROR, 1)

#define FOREACH_BOOL(V) \
  V(FALSE, 0)           \
  V(TRUE, 1)

#define FOREACH_CGB_FLAG(V)   \
  V(CGB_FLAG_NONE, 0)         \
  V(CGB_FLAG_SUPPORTED, 0x80) \
  V(CGB_FLAG_REQUIRED, 0xC0)

#define FOREACH_SGB_FLAG(V) \
  V(SGB_FLAG_NONE, 0)       \
  V(SGB_FLAG_SUPPORTED, 3)

#define FOREACH_CARTRIDGE_TYPE(V)                 \
  V(CARTRIDGE_TYPE_ROM_ONLY, 0x0)                 \
  V(CARTRIDGE_TYPE_MBC1, 0x1)                     \
  V(CARTRIDGE_TYPE_MBC1_RAM, 0x2)                 \
  V(CARTRIDGE_TYPE_MBC1_RAM_BATTERY, 0x3)         \
  V(CARTRIDGE_TYPE_MBC2, 0x5)                     \
  V(CARTRIDGE_TYPE_MBC2_BATTERY, 0x6)             \
  V(CARTRIDGE_TYPE_ROM_RAM, 0x8)                  \
  V(CARTRIDGE_TYPE_ROM_RAM_BATTERY, 0x9)          \
  V(CARTRIDGE_TYPE_MMM01, 0xb)                    \
  V(CARTRIDGE_TYPE_MMM01_RAM, 0xc)                \
  V(CARTRIDGE_TYPE_MMM01_RAM_BATTERY, 0xd)        \
  V(CARTRIDGE_TYPE_MBC3_TIMER_BATTERY, 0xf)       \
  V(CARTRIDGE_TYPE_MBC3_TIMER_RAM_BATTERY, 0x10)  \
  V(CARTRIDGE_TYPE_MBC3, 0x11)                    \
  V(CARTRIDGE_TYPE_MBC3_RAM, 0x12)                \
  V(CARTRIDGE_TYPE_MBC3_RAM_BATTERY, 0x13)        \
  V(CARTRIDGE_TYPE_MBC4, 0x15)                    \
  V(CARTRIDGE_TYPE_MBC4_RAM, 0x16)                \
  V(CARTRIDGE_TYPE_MBC4_RAM_BATTERY, 0x17)        \
  V(CARTRIDGE_TYPE_MBC5, 0x19)                    \
  V(CARTRIDGE_TYPE_MBC5_RAM, 0x1a)                \
  V(CARTRIDGE_TYPE_MBC5_RAM_BATTERY, 0x1b)        \
  V(CARTRIDGE_TYPE_MBC5_RUMBLE, 0x1c)             \
  V(CARTRIDGE_TYPE_MBC5_RUMBLE_RAM, 0x1d)         \
  V(CARTRIDGE_TYPE_MBC5_RUMBLE_RAM_BATTERY, 0x1e) \
  V(CARTRIDGE_TYPE_POCKET_CAMERA, 0xfc)           \
  V(CARTRIDGE_TYPE_BANDAI_TAMA5, 0xfd)            \
  V(CARTRIDGE_TYPE_HUC3, 0xfe)                    \
  V(CARTRIDGE_TYPE_HUC1_RAM_BATTERY, 0xff)

#define FOREACH_ROM_SIZE(V)  \
  V(ROM_SIZE_32K, 0, 2)      \
  V(ROM_SIZE_64K, 1, 4)      \
  V(ROM_SIZE_128K, 2, 8)     \
  V(ROM_SIZE_256K, 3, 16)    \
  V(ROM_SIZE_512K, 4, 32)    \
  V(ROM_SIZE_1M, 5, 64)      \
  V(ROM_SIZE_2M, 6, 128)     \
  V(ROM_SIZE_4M, 7, 256)     \
  V(ROM_SIZE_1_1M, 0x52, 72) \
  V(ROM_SIZE_1_2M, 0x53, 80) \
  V(ROM_SIZE_1_5M, 0x54, 96)

#define FOREACH_RAM_SIZE(V) \
  V(RAM_SIZE_NONE, 0, 0)    \
  V(RAM_SIZE_2K, 1, 2048)   \
  V(RAM_SIZE_8K, 2, 8192)   \
  V(RAM_SIZE_32K, 3, 32768)

#define FOREACH_DESTINATION_CODE(V) \
  V(DESTINATION_CODE_JAPANESE, 0)   \
  V(DESTINATION_CODE_NON_JAPANESE, 1)

#define DEFINE_ENUM(name, code, ...) name = code,
#define DEFINE_STRING(name, code, ...) [code] = #name,

const char* get_enum_string(const char** strings,
                            size_t string_count,
                            size_t value) {
  return value < string_count ? strings[value] : "unknown";
}

#define DEFINE_NAMED_ENUM(NAME, Name, name, foreach)                 \
  enum Name { foreach (DEFINE_ENUM) NAME##_COUNT };                  \
  enum Result is_##name##_valid(enum Name value) {                   \
    return value < NAME##_COUNT;                                     \
  }                                                                  \
  const char* get_##name##_string(enum Name value) {                 \
    static const char* s_strings[] = {foreach (DEFINE_STRING)};      \
    return get_enum_string(s_strings, ARRAY_SIZE(s_strings), value); \
  }

DEFINE_NAMED_ENUM(RESULT, Result, result, FOREACH_RESULT)
DEFINE_NAMED_ENUM(BOOL, Bool, bool, FOREACH_BOOL)
DEFINE_NAMED_ENUM(CGB_FLAG, CgbFlag, cgb_flag, FOREACH_CGB_FLAG)
DEFINE_NAMED_ENUM(SGB_FLAG, SgbFlag, sgb_flag, FOREACH_SGB_FLAG)
DEFINE_NAMED_ENUM(CARTRIDGE_TYPE,
                  CartridgeType,
                  cartridge_type,
                  FOREACH_CARTRIDGE_TYPE)
DEFINE_NAMED_ENUM(ROM_SIZE, RomSize, rom_size, FOREACH_ROM_SIZE)
DEFINE_NAMED_ENUM(RAM_SIZE, RamSize, ram_size, FOREACH_RAM_SIZE)
DEFINE_NAMED_ENUM(DESTINATION_CODE,
                  DestinationCode,
                  destination_code,
                  FOREACH_DESTINATION_CODE)

enum IOReg {
#define V(name, offset) IO_##name##_ADDR = 0xff00 + offset,
  FOREACH_IO_REG(V)
#undef V
};

const char* get_io_reg_string(Address addr) {
  assert(addr >= 0xff00);
  static const char* s_strings[] = {FOREACH_IO_REG(DEFINE_STRING)};
  uint8_t offset = addr - 0xff00;
  const char* result =
      get_enum_string(s_strings, ARRAY_SIZE(s_strings), offset);
  return result != NULL ? result : "???";
}

uint32_t s_rom_bank_size[] = {
#define V(name, code, bank_size) [code] = bank_size,
    FOREACH_ROM_SIZE(V)
#undef V
};

enum MemoryMapType {
  MEMORY_MAP_ROM,
  MEMORY_MAP_ROM_BANK_SWITCH,
  MEMORY_MAP_VRAM,
  MEMORY_MAP_EXTERNAL_RAM,
  MEMORY_MAP_WORK_RAM,
  MEMORY_MAP_WORK_RAM_BANK_SWITCH,
  MEMORY_MAP_OAM,
  MEMORY_MAP_UNUSED,
  MEMORY_MAP_IO,
  MEMORY_MAP_HIGH_RAM,
};

enum BankMode {
  BANK_MODE_ROM = 0,
  BANK_MODE_RAM = 1,
};

enum JoypadSelect {
  JOYPAD_SELECT_BOTH = 0,
  JOYPAD_SELECT_BUTTONS = 1,
  JOYPAD_SELECT_DPAD = 2,
  JOYPAD_SELECT_NONE = 3,
};

enum TimerClock {
  TIMER_CLOCK_4096_HZ = 0,
  TIMER_CLOCK_262144_HZ = 1,
  TIMER_CLOCK_65536_HZ = 2,
  TIMER_CLOCK_16384_HZ = 3,
};

enum SweepDirection {
  SWEEP_DIRECTION_ADDITION = 0,
  SWEEP_DIRECTION_SUBTRACTION = 1,
};

enum EnvelopeDirection {
  ENVELOPE_ATTENUATE = 0,
  ENVELOPE_AMPLIFY = 1,
};

enum WaveDuty {
  WAVE_DUTY_12_5 = 0,
  WAVE_DUTY_25 = 1,
  WAVE_DUTY_50 = 2,
  WAVE_DUTY_75 = 3,
};

enum OutputLevel {
  OUTPUT_LEVEL_MUTE = 0,
  OUTPUT_LEVEL_100 = 1,
  OUTPUT_LEVEL_50 = 2,
  OUTPUT_LEVEL_25 = 3,
};

enum CounterStep {
  COUNTER_STEP_15 = 0,
  COUNTER_STEP_7 = 1,
};

enum TileMapSelect {
  TILE_MAP_9800_9BFF = 0,
  TILE_MAP_9C00_9FFF = 1,
};

enum TileDataSelect {
  TILE_DATA_8800_97FF = 0,
  TILE_DATA_8000_8FFF = 1,
};

enum ObjSize {
  OBJ_SIZE_8X8 = 0,
  OBJ_SIZE_8X16 = 1,
};
static uint8_t s_obj_size_to_height[] = {
  [OBJ_SIZE_8X8] = 8,
  [OBJ_SIZE_8X16] = 16,
};

enum LCDMode {
  LCD_MODE_HBLANK = 0,         /* LCD mode 0 */
  LCD_MODE_VBLANK = 1,         /* LCD mode 1 */
  LCD_MODE_USING_OAM = 2,      /* LCD mode 2 */
  LCD_MODE_USING_OAM_VRAM = 3, /* LCD mode 3 */
};

enum Color {
  COLOR_WHITE = 0,
  COLOR_LIGHT_GRAY = 1,
  COLOR_DARK_GRAY = 2,
  COLOR_BLACK = 3,
};
static RGBA s_color_to_rgba[] = {
  [COLOR_WHITE] = RGBA_WHITE,
  [COLOR_LIGHT_GRAY] = RGBA_LIGHT_GRAY,
  [COLOR_DARK_GRAY] = RGBA_DARK_GRAY,
  [COLOR_BLACK] = RGBA_BLACK,
};
static uint8_t s_color_to_obj_mask[] = {
  [COLOR_WHITE] = 0xff,
  [COLOR_LIGHT_GRAY] = 0,
  [COLOR_DARK_GRAY] = 0,
  [COLOR_BLACK] = 0,
};

enum ObjPriority {
  OBJ_PRIORITY_ABOVE_BG = 0,
  OBJ_PRIORITY_BEHIND_BG = 1,
};

struct RomData {
  uint8_t* data;
  size_t size;
};

struct WorkRam {
  uint8_t data[WORK_RAM_SIZE];
  size_t size; /* normally 8k, 32k in CGB mode */
};

struct StringSlice {
  const char* start;
  size_t length;
};

struct RomInfo {
  struct StringSlice title;
  struct StringSlice manufacturer;
  enum CgbFlag cgb_flag;
  struct StringSlice new_licensee;
  uint8_t old_licensee_code;
  enum SgbFlag sgb_flag;
  enum CartridgeType cartridge_type;
  enum RomSize rom_size;
  uint32_t rom_banks;
  enum RamSize ram_size;
  enum DestinationCode destination_code;
  uint8_t header_checksum;
  uint16_t global_checksum;
  enum Result header_checksum_valid;
  enum Result global_checksum_valid;
};

struct Emulator;

struct MemoryMap {
  uint32_t rom_bank;
  uint32_t ram_bank;
  enum Bool ram_enabled;
  enum BankMode bank_mode;
  uint8_t (*read_rom_bank_switch)(struct Emulator*, MaskedAddress);
  uint8_t (*read_work_ram_bank_switch)(struct Emulator*, MaskedAddress);
  uint8_t (*read_external_ram)(struct Emulator*, MaskedAddress);
  void (*write_rom)(struct Emulator*, MaskedAddress, uint8_t);
  void (*write_work_ram_bank_switch)(struct Emulator*, MaskedAddress, uint8_t);
  void (*write_external_ram)(struct Emulator*, MaskedAddress, uint8_t);
};

struct MemoryTypeAddressPair {
  enum MemoryMapType type;
  MaskedAddress addr;
};

/* TODO(binji): endianness */
#define REGISTER_PAIR(X, Y) \
  union {                   \
    struct {                \
      uint8_t Y;            \
      uint8_t X;            \
    };                      \
    uint16_t X##Y;          \
  }

struct Registers {
  uint8_t A;
  REGISTER_PAIR(B, C);
  REGISTER_PAIR(D, E);
  REGISTER_PAIR(H, L);
  uint16_t SP;
  uint16_t PC;
  struct {
    enum Bool Z;
    enum Bool N;
    enum Bool H;
    enum Bool C;
  } flags;
};

typedef uint8_t Tile[TILE_WIDTH * TILE_HEIGHT];
typedef uint8_t TileMap[TILE_MAP_WIDTH * TILE_MAP_HEIGHT];

struct VideoRam {
  Tile tile[TILE_COUNT];
  TileMap map[MAP_COUNT];
  uint8_t data[VIDEO_RAM_SIZE];
};

struct Palette {
  enum Color color[PALETTE_COLOR_COUNT];
};

struct Obj {
  uint8_t y;
  uint8_t x;
  uint8_t tile;
  enum ObjPriority priority;
  enum Bool yflip;
  enum Bool xflip;
  uint8_t palette;
};

struct OAM {
  struct Obj objs[OBJ_COUNT];
  struct Palette obp[OBJ_PALETTE_COUNT];
};

struct Joypad {
  enum Bool down, up, left, right;
  enum Bool start, select, B, A;
  enum JoypadSelect joypad_select;
};

struct Interrupts {
  enum Bool IME; /* Interrupt Master Enable */
  uint8_t IE;    /* Interrupt Enable */
  uint8_t IF;    /* Interrupt Request */
};

struct Timer {
  uint8_t DIV;  /* Incremented at 16384 Hz */
  uint8_t TIMA; /* Incremented at rate defined by input_clock_select */
  uint8_t TMA;  /* When TIMA overflows, it is set to this value */
  enum TimerClock input_clock_select; /* Select the rate of TIMA */
  uint32_t div_cycles;
  uint32_t tima_cycles;
  enum Bool on;
};

struct Serial {
  enum Bool transfer_start;
  enum Bool clock_speed;
  enum Bool shift_clock;
};

struct Channel {
  uint8_t sweep_time;                        /* Channel 1 */
  enum SweepDirection sweep_direction;       /* Channel 1 */
  uint8_t sweep_count;                       /* Channel 1 */
  enum WaveDuty wave_duty;                   /* Channel 1, 2 */
  uint8_t sound_length;                      /* All channels */
  uint8_t initial_volume;                    /* Channel 1, 2, 4 */
  enum EnvelopeDirection envelope_direction; /* Channel 1, 2, 4 */
  uint8_t envelope_count;                    /* Channel 1, 2, 4 */
  uint16_t frequency;                        /* Channel 1, 2, 3 */
  enum Bool consecutive;                     /* All channels */
  enum Bool initial;                         /* All channels */
  enum Bool on;                              /* Channel 3 */
  enum OutputLevel output_level;             /* Channel 3 */
  uint8_t shift_clock_frequency;             /* Channel 4 */
  enum CounterStep counter_step;             /* Channel 4 */
  uint8_t divide_ratio;                      /* Channel 4 */
};

struct Sound {
  uint8_t so2_volume;
  uint8_t so1_volume;
  enum Bool so2_output[SOUND_COUNT];
  enum Bool so1_output[SOUND_COUNT];
  enum Bool enabled;
  enum Bool sound_on[SOUND_COUNT];
  struct Channel channel[CHANNEL_COUNT];
};

struct LCDControl {
  enum Bool display;
  enum TileMapSelect window_tile_map_select;
  enum Bool window_display;
  enum TileDataSelect bg_tile_data_select;
  enum TileMapSelect bg_tile_map_select;
  enum ObjSize obj_size;
  enum Bool obj_display;
  enum Bool bg_display;
};

struct LCDStatus {
  enum Bool y_compare_intr;
  enum Bool using_oam_intr;
  enum Bool vblank_intr;
  enum Bool hblank_intr;
  enum LCDMode mode;
};

struct LCD {
  struct LCDControl lcdc; /* LCD control */
  struct LCDStatus stat;  /* LCD status */
  uint8_t SCY;            /* Screen Y */
  uint8_t SCX;            /* Screen X */
  uint8_t LY;             /* Line Y */
  uint8_t LYC;            /* Line Y Compare */
  uint8_t WY;             /* Window Y */
  uint8_t WX;             /* Window X */
  struct Palette bgp;     /* BG Palette */
  uint32_t cycles;
  uint32_t frame;
  uint8_t win_y;    /* The window Y is only incremented when rendered. */
  uint8_t frame_WY; /* WY is cached per frame. */
  enum Bool new_frame_edge;
};

struct DMA {
  enum Bool active;
  Address addr;
  uint32_t cycles;
};

typedef RGBA FrameBuffer[SCREEN_WIDTH * SCREEN_HEIGHT];

struct Emulator {
  struct RomData rom_data;
  struct RomInfo rom_info;
  struct MemoryMap memory_map;
  struct Registers reg;
  struct VideoRam vram;
  struct WorkRam ram;
  struct Interrupts interrupts;
  struct OAM oam;
  struct Joypad joypad;
  struct Serial serial;
  struct Timer timer;
  struct Sound sound;
  struct LCD lcd;
  struct DMA dma;
  uint8_t hram[HIGH_RAM_SIZE];
  FrameBuffer frame_buffer;
};

enum Bool s_trace = FALSE;
uint32_t s_trace_counter = 0;

void print_emulator_info(struct Emulator*);
void write_io(struct Emulator*, Address, uint8_t);

enum Result read_rom_data_from_file(const char* filename,
                                    struct RomData* out_rom_data) {
  FILE* f = fopen(filename, "rb");
  CHECK_MSG(f, "unable to open file \"%s\".\n", filename);
  CHECK_MSG(fseek(f, 0, SEEK_END) >= 0, "fseek to end failed.\n");
  long size = ftell(f);
  CHECK_MSG(size >= 0, "ftell failed.");
  CHECK_MSG(fseek(f, 0, SEEK_SET) >= 0, "fseek to beginning failed.\n");
  CHECK_MSG(size >= MINIMUM_ROM_SIZE, "size < minimum rom size (%u).\n",
            MINIMUM_ROM_SIZE);
  uint8_t* data = malloc(size);
  CHECK_MSG(data, "allocation failed.");
  CHECK_MSG(fread(data, size, 1, f) == 1, "fread failed.\n");
  fclose(f);
  out_rom_data->data = data;
  out_rom_data->size = size;
  return OK;
error:
  if (f) {
    fclose(f);
  }
  return ERROR;
}

void get_rom_title(struct RomData* rom_data, struct StringSlice* out_title) {
  const char* start = (char*)rom_data->data + TITLE_START_ADDR;
  const char* end = start + TITLE_END_ADDR;
  const char* p = start;
  size_t length = 0;
  while (p <= end && *p != 0 && (*p & 0x80) == 0) {
    length++;
    p++;
  }
  out_title->start = start;
  out_title->length = length;
}

void get_manufacturer_code(struct RomData* rom_data,
                           struct StringSlice* out_manufacturer) {
  const char* start = (char*)rom_data->data + MANUFACTURERS_CODE_START_ADDR;
  const char* end = start + MANUFACTURERS_CODE_END_ADDR;
  if (*(start - 1) != 0) {
    /* The title is too long, so this must be a ROM without a manufacturer's
     * code */
    out_manufacturer->start = "";
    out_manufacturer->length = 0;
    return;
  }

  const char* p = start;
  size_t length = 0;
  while (p <= end && *p != 0) {
    length++;
    p++;
  }
  out_manufacturer->start = start;
  out_manufacturer->length = length;
}

void get_new_licensee(struct RomData* rom_data,
                      uint8_t old_licensee_code,
                      struct StringSlice* out_licensee) {
  if (old_licensee_code == NEW_LICENSEE_CODE) {
    out_licensee->start =
        (const char*)rom_data->data + NEW_LINCENSEE_CODE_START_ADDR;
    out_licensee->length =
        NEW_LINCENSEE_CODE_END_ADDR - NEW_LINCENSEE_CODE_START_ADDR + 1;
  } else {
    out_licensee->start = "";
    out_licensee->length = 0;
  }
}

enum Result validate_header_checksum(struct RomData* rom_data) {
  uint8_t expected_checksum = ROM_U8(uint8_t, HEADER_CHECKSUM_ADDR);
  uint8_t checksum = 0;
  size_t i = 0;
  for (i = HEADER_CHECKSUM_RANGE_START; i <= HEADER_CHECKSUM_RANGE_END; ++i) {
    checksum = checksum - rom_data->data[i] - 1;
  }
  return checksum == expected_checksum ? OK : ERROR;
}

enum Result validate_global_checksum(struct RomData* rom_data) {
  uint16_t expected_checksum = ROM_U16_BE(GLOBAL_CHECKSUM_START_ADDR);
  uint16_t checksum = 0;
  size_t i = 0;
  for (i = 0; i < rom_data->size; ++i) {
    if (i == GLOBAL_CHECKSUM_START_ADDR || i == GLOBAL_CHECKSUM_END_ADDR)
      continue;
    checksum += rom_data->data[i];
  }
  return checksum == expected_checksum ? OK : ERROR;
}

uint32_t get_rom_bank_count(enum RomSize rom_size) {
  return is_rom_size_valid(rom_size) ? s_rom_bank_size[rom_size] : 0;
}

uint32_t get_rom_byte_size(enum RomSize rom_size) {
  return get_rom_bank_count(rom_size) * ROM_BANK_BYTE_SIZE;
}

enum Result get_rom_info(struct RomData* rom_data,
                         struct RomInfo* out_rom_info) {
  struct RomInfo rom_info;
  ZERO_MEMORY(rom_info);

  rom_info.rom_size = ROM_U8(enum RomSize, ROM_SIZE_ADDR);
  uint32_t rom_byte_size = get_rom_byte_size(rom_info.rom_size);
  CHECK_MSG(rom_data->size == rom_byte_size,
            "Invalid ROM size: expected %u, got %zu.\n", rom_byte_size,
            rom_data->size);

  rom_info.rom_banks = get_rom_bank_count(rom_info.rom_size);

  get_rom_title(rom_data, &rom_info.title);
  get_manufacturer_code(rom_data, &rom_info.manufacturer);
  rom_info.cgb_flag = ROM_U8(enum CgbFlag, CGB_FLAG_ADDR);
  rom_info.sgb_flag = ROM_U8(enum SgbFlag, SGB_FLAG_ADDR);
  rom_info.cartridge_type = ROM_U8(enum CartridgeType, CARTRIDGE_TYPE_ADDR);
  rom_info.ram_size = ROM_U8(enum RamSize, RAM_SIZE_ADDR);
  rom_info.destination_code =
      ROM_U8(enum DestinationCode, DESTINATATION_CODE_ADDR);
  rom_info.old_licensee_code = ROM_U8(uint8_t, OLD_LICENSEE_CODE_ADDR);
  get_new_licensee(rom_data, rom_info.old_licensee_code,
                   &rom_info.new_licensee);
  rom_info.header_checksum = ROM_U8(uint8_t, HEADER_CHECKSUM_ADDR);
  rom_info.header_checksum_valid = validate_header_checksum(rom_data);
  rom_info.global_checksum = ROM_U16_BE(GLOBAL_CHECKSUM_START_ADDR);
  rom_info.global_checksum_valid = validate_global_checksum(rom_data);

  *out_rom_info = rom_info;
  return OK;
error:
  return ERROR;
}

void print_rom_info(struct RomInfo* rom_info) {
  printf("title: \"%.*s\"\n", (int)rom_info->title.length,
         rom_info->title.start);
  printf("manufacturer: \"%.*s\"\n", (int)rom_info->manufacturer.length,
         rom_info->manufacturer.start);
  printf("cgb flag: %s\n", get_cgb_flag_string(rom_info->cgb_flag));
  printf("sgb flag: %s\n", get_sgb_flag_string(rom_info->sgb_flag));
  printf("cartridge type: %s\n",
         get_cartridge_type_string(rom_info->cartridge_type));
  printf("rom size: %s\n", get_rom_size_string(rom_info->rom_size));
  printf("ram size: %s\n", get_ram_size_string(rom_info->ram_size));
  printf("destination code: %s\n",
         get_destination_code_string(rom_info->destination_code));
  printf("old licensee code: %u\n", rom_info->old_licensee_code);
  printf("new licensee: %.*s\n", (int)rom_info->new_licensee.length,
         rom_info->new_licensee.start);

  printf("header checksum: 0x%02x [%s]\n", rom_info->header_checksum,
         get_result_string(rom_info->header_checksum_valid));
  printf("global checksum: 0x%04x [%s]\n", rom_info->global_checksum,
         get_result_string(rom_info->global_checksum_valid));
}

uint8_t rom_only_read_rom_bank_switch(struct Emulator* e, MaskedAddress addr) {
  /* Always return ROM in range 0x4000-0x7fff. */
  assert(addr <= ADDR_MASK_16K);
  addr += 0x4000;
  return e->rom_data.data[addr];
}

void rom_only_write_rom(struct Emulator* e, MaskedAddress addr, uint8_t value) {
  /* TODO(binji): log? */
}

uint8_t gb_read_work_ram_bank_switch(struct Emulator* e, MaskedAddress addr) {
  assert(addr <= ADDR_MASK_4K);
  return e->ram.data[0x1000 + addr];
}

uint8_t mbc1_read_rom_bank_switch(struct Emulator* e, MaskedAddress addr) {
  assert(addr <= ADDR_MASK_16K);
  uint32_t rom_addr = (e->memory_map.rom_bank << ROM_BANK_SHIFT) | addr;
  if (rom_addr < e->rom_data.size) {
    return e->rom_data.data[rom_addr];
  } else {
    LOG("mbc1_read_rom_bank_switch(0x%04x): bad address (bank = %u)!\n", addr,
        e->memory_map.rom_bank);
    return 0;
  }
}

void mbc1_write_rom(struct Emulator* e, MaskedAddress addr, uint8_t value) {
  DEBUG("mbc1_write_rom: 0x%04x <= 0x%02x\n", addr, value);
  switch (addr >> 13) {
    case 0: /* 0000-1fff */
      e->memory_map.ram_enabled =
          (value & MBC1_RAM_ENABLED_MASK) == MBC1_RAM_ENABLED_VALUE;
      break;

    case 1: /* 2000-3fff */
      e->memory_map.rom_bank &= ~MBC1_ROM_BANK_LO_MASK;
      e->memory_map.rom_bank |= value & MBC1_ROM_BANK_LO_MASK;
      /* Banks 0, 0x20, 0x40, 0x60 map to 1, 0x21, 0x41, 0x61 respectively. */
      if (e->memory_map.rom_bank == 0) {
        e->memory_map.rom_bank++;
      }
      break;

    case 2: /* 4000-5fff */
      value &= MBC1_BANK_HI_MASK;
      if (e->memory_map.bank_mode == BANK_MODE_ROM) {
        e->memory_map.rom_bank &= ~MBC1_ROM_BANK_LO_MASK;
        e->memory_map.rom_bank |= value << MBC1_BANK_HI_SHIFT;
      } else {
        e->memory_map.ram_bank = value;
      }
      break;

    case 3: { /* 6000-7fff */
      enum BankMode new_bank_mode = (enum BankMode)(value & 1);
      if (e->memory_map.bank_mode != new_bank_mode) {
        if (new_bank_mode == BANK_MODE_ROM) {
          /* Use the ram bank bits as the high rom bank bits. */
          assert(e->memory_map.rom_bank <= MBC1_ROM_BANK_LO_MASK);
          e->memory_map.rom_bank |= e->memory_map.ram_bank
                                    << MBC1_BANK_HI_SHIFT;
          e->memory_map.ram_bank = 0;
        } else {
          /* Use the high rom bank bits as the ram bank bits. */
          e->memory_map.ram_bank = e->memory_map.rom_bank >> MBC1_BANK_HI_SHIFT;
          assert(e->memory_map.ram_bank <= MBC1_BANK_HI_MASK);
          e->memory_map.rom_bank &= ~MBC1_ROM_BANK_LO_MASK;
        }
        e->memory_map.bank_mode = new_bank_mode;
      }
      break;
    }

    default:
      UNREACHABLE("invalid addr: 0x%04x\n", addr);
      break;
  }
}

void gb_write_work_ram_bank_switch(struct Emulator* e,
                                   MaskedAddress addr,
                                   uint8_t value) {
  assert(addr <= ADDR_MASK_4K);
  e->ram.data[0x1000 + addr] = value;
}

uint8_t dummy_read_external_ram(struct Emulator* e, MaskedAddress addr) {
  return 0;
}

void dummy_write_external_ram(struct Emulator* e,
                              MaskedAddress addr,
                              uint8_t value) {}

struct MemoryMap s_rom_only_memory_map = {
  .read_rom_bank_switch = rom_only_read_rom_bank_switch,
  .read_work_ram_bank_switch = gb_read_work_ram_bank_switch,
  .read_external_ram = dummy_read_external_ram,
  .write_rom = rom_only_write_rom,
  .write_work_ram_bank_switch = gb_write_work_ram_bank_switch,
  .write_external_ram = dummy_write_external_ram,
};

struct MemoryMap s_mbc1_memory_map = {
  .rom_bank = 1,
  .ram_bank = 0,
  .ram_enabled = FALSE,
  .bank_mode = BANK_MODE_ROM,
  .read_rom_bank_switch = mbc1_read_rom_bank_switch,
  .read_work_ram_bank_switch = gb_read_work_ram_bank_switch,
  .read_external_ram = dummy_read_external_ram,
  .write_rom = mbc1_write_rom,
  .write_work_ram_bank_switch = gb_write_work_ram_bank_switch,
  .write_external_ram = dummy_write_external_ram,
};

enum Result get_memory_map(struct RomInfo* rom_info,
                           struct MemoryMap* out_memory_map) {
  switch (rom_info->cartridge_type) {
    case CARTRIDGE_TYPE_ROM_ONLY:
      *out_memory_map = s_rom_only_memory_map;
      return OK;

    case CARTRIDGE_TYPE_MBC1:
    case CARTRIDGE_TYPE_MBC1_RAM:
    case CARTRIDGE_TYPE_MBC1_RAM_BATTERY:
      *out_memory_map = s_mbc1_memory_map;
      return OK;

    default:
      NOT_IMPLEMENTED_NO_EMULATOR(
          "memory map for %s not implemented.\n",
          get_cartridge_type_string(rom_info->cartridge_type));
  }
  return ERROR;
}

uint8_t get_f_reg(struct Registers* reg) {
  return TO_REG(reg->flags.Z, CPU_FLAG_Z) |
         TO_REG(reg->flags.N, CPU_FLAG_N) |
         TO_REG(reg->flags.H, CPU_FLAG_H) |
         TO_REG(reg->flags.C, CPU_FLAG_C);
}

uint16_t get_af_reg(struct Registers* reg) {
  return (reg->A << 8) | get_f_reg(reg);
}

void set_af_reg(struct Registers* reg, uint16_t af) {
  reg->A = af >> 8;
  reg->flags.Z = FROM_REG(af, CPU_FLAG_Z);
  reg->flags.N = FROM_REG(af, CPU_FLAG_N);
  reg->flags.H = FROM_REG(af, CPU_FLAG_H);
  reg->flags.C = FROM_REG(af, CPU_FLAG_C);
}

enum Result init_emulator(struct Emulator* e, struct RomData* rom_data) {
  ZERO_MEMORY(*e);
  e->rom_data = *rom_data;
  CHECK(SUCCESS(get_rom_info(rom_data, &e->rom_info)));
#if 1
  print_rom_info(&e->rom_info);
#endif
  CHECK(SUCCESS(get_memory_map(&e->rom_info, &e->memory_map)));
  set_af_reg(&e->reg, 0x01b0);
  e->reg.BC = 0x0013;
  e->reg.DE = 0x00d8;
  e->reg.HL = 0x014d;
  e->reg.SP = 0xfffe;
  e->reg.PC = 0x0100;
  e->interrupts.IME = TRUE;
  write_io(e, IO_NR10_ADDR, 0x80);
  write_io(e, IO_NR11_ADDR, 0xbf);
  write_io(e, IO_NR12_ADDR, 0xf3);
  write_io(e, IO_NR14_ADDR, 0xbf);
  write_io(e, IO_NR21_ADDR, 0x3f);
  write_io(e, IO_NR22_ADDR, 0x00);
  write_io(e, IO_NR24_ADDR, 0xbf);
  write_io(e, IO_NR30_ADDR, 0x7f);
  write_io(e, IO_NR31_ADDR, 0xff);
  write_io(e, IO_NR32_ADDR, 0x9f);
  write_io(e, IO_NR33_ADDR, 0xbf);
  write_io(e, IO_NR41_ADDR, 0xff);
  write_io(e, IO_NR42_ADDR, 0x00);
  write_io(e, IO_NR43_ADDR, 0x00);
  write_io(e, IO_NR44_ADDR, 0xbf);
  write_io(e, IO_NR50_ADDR, 0x77);
  write_io(e, IO_NR51_ADDR, 0xf3);
  write_io(e, IO_NR52_ADDR, 0xf1);
  write_io(e, IO_LCDC_ADDR, 0x91);
  write_io(e, IO_SCY_ADDR, 0x00);
  write_io(e, IO_SCX_ADDR, 0x00);
  write_io(e, IO_LYC_ADDR, 0x00);
  write_io(e, IO_BGP_ADDR, 0xfc);
  write_io(e, IO_OBP0_ADDR, 0xff);
  write_io(e, IO_OBP1_ADDR, 0xff);
  write_io(e, IO_IE_ADDR, 0x0);
  return OK;
error:
  return ERROR;
}

struct MemoryTypeAddressPair map_address(Address addr) {
  struct MemoryTypeAddressPair result;
  switch (addr >> 12) {
    case 0x0:
    case 0x1:
    case 0x2:
    case 0x3:
      result.type = MEMORY_MAP_ROM;
      result.addr = addr & ADDR_MASK_16K;
      break;

    case 0x4:
    case 0x5:
    case 0x6:
    case 0x7:
      result.type = MEMORY_MAP_ROM_BANK_SWITCH;
      result.addr = addr & ADDR_MASK_16K;
      break;

    case 0x8:
    case 0x9:
      result.type = MEMORY_MAP_VRAM;
      result.addr = addr & ADDR_MASK_8K;
      break;

    case 0xA:
    case 0xB:
      result.type = MEMORY_MAP_EXTERNAL_RAM;
      result.addr = addr & ADDR_MASK_8K;
      break;

    case 0xC:
    case 0xE: /* mirror of 0xc000..0xcfff */
      result.type = MEMORY_MAP_WORK_RAM;
      result.addr = addr & ADDR_MASK_4K;
      break;

    case 0xD:
      result.type = MEMORY_MAP_WORK_RAM_BANK_SWITCH;
      result.addr = addr & ADDR_MASK_4K;
      break;

    case 0xF:
      if (addr < OAM_START_ADDR) {
        /* 0xf000 - 0xfdff: mirror of 0xd000-0xddff */
        result.type = MEMORY_MAP_WORK_RAM_BANK_SWITCH;
        result.addr = addr & ADDR_MASK_4K;
      } else if (addr <= OAM_END_ADDR) {
        /* 0xfe00 - 0xfe9f */
        result.type = MEMORY_MAP_OAM;
        result.addr = addr - OAM_START_ADDR;
      } else if (addr <= UNUSED_END_ADDR) {
        /* 0xfea0 - 0xfeff */
        result.type = MEMORY_MAP_UNUSED;
        result.addr = addr;
      } else if (addr <= IO_END_ADDR) {
        /* 0xff00 - 0xff7f */
        result.type = MEMORY_MAP_IO;
        result.addr = addr;
      } else if (addr <= HIGH_RAM_END_ADDR) {
        /* 0xff80 - 0xfffe */
        result.type = MEMORY_MAP_HIGH_RAM;
        result.addr = addr - HIGH_RAM_START_ADDR;
      } else {
        /* 0xffff: IE register */
        result.type = MEMORY_MAP_IO;
        result.addr = addr;
      }
      break;
  }
  return result;
}


uint8_t read_vram(struct Emulator* e, MaskedAddress addr) {
  if (e->lcd.lcdc.display && e->lcd.stat.mode == LCD_MODE_USING_OAM_VRAM) {
    /* return garbage if read while the io is using vram */
    return 0xff;
  } else {
    assert(addr <= ADDR_MASK_8K);
    return e->vram.data[addr];
  }
}

uint8_t read_oam(struct Emulator* e, MaskedAddress addr) {
  uint8_t obj_index = addr >> 2;
  struct Obj* obj = &e->oam.objs[obj_index];
  switch (addr & 3) {
    case 0: return obj->y + OBJ_Y_OFFSET;
    case 1: return obj->x + OBJ_X_OFFSET;
    case 2: return obj->tile;
    case 3:
      return FROM_REG(obj->priority, OBJ_PRIORITY) |
             FROM_REG(obj->yflip, OBJ_YFLIP) | FROM_REG(obj->xflip, OBJ_XFLIP) |
             FROM_REG(obj->palette, OBJ_PALETTE);
  }
  UNREACHABLE("invalid OAM address: 0x%04x\n", addr);
}

uint8_t to_nrx1_reg(struct Channel* channel) {
  return TO_REG(channel->wave_duty, NRX1_WAVE_DUTY);
}

uint8_t to_nrx2_reg(struct Channel* channel) {
  return TO_REG(channel->initial_volume, NRX2_INITIAL_VOLUME) |
         TO_REG(channel->envelope_direction, NRX2_ENVELOPE_DIRECTION) |
         TO_REG(channel->envelope_count, NRX2_ENVELOPE_COUNT);
}

uint8_t to_nrx4_reg(struct Channel* channel) {
  return TO_REG(channel->consecutive, NRX4_COUNTER_CONSECUTIVE);
}

uint8_t read_io(struct Emulator* e, Address addr) {
  switch (addr) {
    case IO_JOYP_ADDR: {
      uint8_t result = 0;
      /* TODO is this the correct behavior when both select bits are low? */
      if (e->joypad.joypad_select == JOYPAD_SELECT_BUTTONS ||
          e->joypad.joypad_select == JOYPAD_SELECT_BOTH) {
        result |= TO_REG(e->joypad.start, JOYP_BUTTON_START) |
                  TO_REG(e->joypad.select, JOYP_BUTTON_SELECT) |
                  TO_REG(e->joypad.B, JOYP_BUTTON_B) |
                  TO_REG(e->joypad.A, JOYP_BUTTON_A);
      }

      if (e->joypad.joypad_select == JOYPAD_SELECT_DPAD ||
          e->joypad.joypad_select == JOYPAD_SELECT_BOTH) {
        result |= TO_REG(e->joypad.down, JOYP_DPAD_DOWN) |
                  TO_REG(e->joypad.up, JOYP_DPAD_UP) |
                  TO_REG(e->joypad.left, JOYP_DPAD_LEFT) |
                  TO_REG(e->joypad.right, JOYP_DPAD_RIGHT);
      }

      /* The bits are low when the buttons are pressed. */
      return TO_REG(e->joypad.joypad_select, JOYP_JOYPAD_SELECT) | ~result;
    }
    case IO_SB_ADDR:
      return 0; /* TODO */
    case IO_SC_ADDR:
      return TO_REG(e->serial.transfer_start, SC_TRANSFER_START) |
             TO_REG(e->serial.clock_speed, SC_CLOCK_SPEED) |
             TO_REG(e->serial.shift_clock, SC_SHIFT_CLOCK);
    case IO_DIV_ADDR:
      return e->timer.DIV;
    case IO_TIMA_ADDR:
      return e->timer.TIMA;
    case IO_TMA_ADDR:
      return e->timer.TMA;
    case IO_TAC_ADDR:
      return TO_REG(e->timer.on, TAC_TIMER_ON) |
             TO_REG(e->timer.input_clock_select, TAC_INPUT_CLOCK_SELECT);
    case IO_IF_ADDR:
      return e->interrupts.IF;
    case IO_NR10_ADDR: {
      struct Channel* channel = &e->sound.channel[CHANNEL1];
      return TO_REG(channel->sweep_time, NR10_SWEEP_TIME) |
             TO_REG(channel->sweep_direction, NR10_SWEEP_DIRECTION) |
             TO_REG(channel->sweep_count, NR10_SWEEP_COUNT);
    }
    case IO_NR11_ADDR:
      return to_nrx1_reg(&e->sound.channel[CHANNEL1]);
    case IO_NR12_ADDR:
      return to_nrx2_reg(&e->sound.channel[CHANNEL1]);
    case IO_NR13_ADDR:
      return 0; /* Write only */
    case IO_NR14_ADDR:
      return to_nrx4_reg(&e->sound.channel[CHANNEL1]);
    case IO_NR21_ADDR:
      return to_nrx1_reg(&e->sound.channel[CHANNEL2]);
    case IO_NR22_ADDR:
      return to_nrx2_reg(&e->sound.channel[CHANNEL2]);
    case IO_NR23_ADDR:
      return 0; /* Write only */
    case IO_NR24_ADDR:
      return to_nrx4_reg(&e->sound.channel[CHANNEL2]);
    case IO_NR30_ADDR:
      return TO_REG(e->sound.channel[CHANNEL3].on, NR30_SOUND_ON);
    case IO_NR31_ADDR:
      return e->sound.channel[CHANNEL3].sound_length;
    case IO_NR32_ADDR:
      return TO_REG(e->sound.channel[CHANNEL3].output_level,
                    NR32_SELECT_OUTPUT_LEVEL);
    case IO_NR33_ADDR:
      return 0; /* Write only */
    case IO_NR34_ADDR:
      return to_nrx4_reg(&e->sound.channel[CHANNEL3]);
    case IO_NR41_ADDR:
      return TO_REG(e->sound.channel[CHANNEL4].sound_length, NRX1_SOUND_LENGTH);
    case IO_NR42_ADDR:
      return to_nrx2_reg(&e->sound.channel[CHANNEL4]);
    case IO_NR43_ADDR: {
      struct Channel* channel = &e->sound.channel[CHANNEL4];
      return TO_REG(channel->shift_clock_frequency,
                    NR43_SHIFT_CLOCK_FREQUENCY) |
             TO_REG(channel->counter_step, NR43_COUNTER_STEP) |
             TO_REG(channel->divide_ratio, NR43_DIVIDE_RATIO);
    }
    case IO_NR44_ADDR:
      return to_nrx4_reg(&e->sound.channel[CHANNEL4]);
    case IO_NR50_ADDR:
      return TO_REG(e->sound.so2_output[VIN], NR50_VIN_SO2) |
             TO_REG(e->sound.so2_volume, NR50_SO2_VOLUME) |
             TO_REG(e->sound.so1_output[VIN], NR50_VIN_SO1) |
             TO_REG(e->sound.so1_volume, NR50_SO1_VOLUME);
    case IO_NR51_ADDR:
      return TO_REG(e->sound.so2_output[SOUND4], NR51_SOUND4_SO2) |
             TO_REG(e->sound.so2_output[SOUND3], NR51_SOUND3_SO2) |
             TO_REG(e->sound.so2_output[SOUND2], NR51_SOUND2_SO2) |
             TO_REG(e->sound.so2_output[SOUND1], NR51_SOUND1_SO2) |
             TO_REG(e->sound.so1_output[SOUND4], NR51_SOUND4_SO1) |
             TO_REG(e->sound.so1_output[SOUND3], NR51_SOUND3_SO1) |
             TO_REG(e->sound.so1_output[SOUND2], NR51_SOUND2_SO1) |
             TO_REG(e->sound.so1_output[SOUND1], NR51_SOUND1_SO1);
    case IO_NR52_ADDR:
      return TO_REG(e->sound.enabled, NR52_ALL_SOUND_ENABLED) |
             TO_REG(e->sound.sound_on[SOUND4], NR52_SOUND4_ON) |
             TO_REG(e->sound.sound_on[SOUND3], NR52_SOUND3_ON) |
             TO_REG(e->sound.sound_on[SOUND2], NR52_SOUND2_ON) |
             TO_REG(e->sound.sound_on[SOUND1], NR52_SOUND1_ON);
    case IO_LCDC_ADDR:
      return TO_REG(e->lcd.lcdc.display, LCDC_DISPLAY) |
             TO_REG(e->lcd.lcdc.window_tile_map_select,
                    LCDC_WINDOW_TILE_MAP_SELECT) |
             TO_REG(e->lcd.lcdc.window_display, LCDC_WINDOW_DISPLAY) |
             TO_REG(e->lcd.lcdc.bg_tile_data_select, LCDC_BG_TILE_DATA_SELECT) |
             TO_REG(e->lcd.lcdc.bg_tile_map_select, LCDC_BG_TILE_MAP_SELECT) |
             TO_REG(e->lcd.lcdc.obj_size, LCDC_OBJ_SIZE) |
             TO_REG(e->lcd.lcdc.obj_display, LCDC_OBJ_DISPLAY) |
             TO_REG(e->lcd.lcdc.bg_display, LCDC_BG_DISPLAY);
    case IO_STAT_ADDR:
      return TO_REG(e->lcd.stat.y_compare_intr, STAT_YCOMPARE_INTR) |
             TO_REG(e->lcd.stat.using_oam_intr, STAT_USING_OAM_INTR) |
             TO_REG(e->lcd.stat.vblank_intr, STAT_VBLANK_INTR) |
             TO_REG(e->lcd.stat.hblank_intr, STAT_HBLANK_INTR) |
             TO_REG(e->lcd.LY == e->lcd.LYC, STAT_YCOMPARE) |
             TO_REG(e->lcd.stat.mode, STAT_MODE);
    case IO_SCY_ADDR:
      return e->lcd.SCY;
    case IO_SCX_ADDR:
      return e->lcd.SCX;
    case IO_LY_ADDR:
      return e->lcd.LY;
    case IO_LYC_ADDR:
      return e->lcd.LYC;
    case IO_DMA_ADDR:
      return 0; /* Write only. */
    case IO_BGP_ADDR:
      return TO_REG(e->lcd.bgp.color[3], PALETTE_COLOR3) |
             TO_REG(e->lcd.bgp.color[2], PALETTE_COLOR2) |
             TO_REG(e->lcd.bgp.color[1], PALETTE_COLOR1) |
             TO_REG(e->lcd.bgp.color[0], PALETTE_COLOR0);
    case IO_OBP0_ADDR:
      return TO_REG(e->oam.obp[0].color[3], PALETTE_COLOR3) |
             TO_REG(e->oam.obp[0].color[2], PALETTE_COLOR2) |
             TO_REG(e->oam.obp[0].color[1], PALETTE_COLOR1);
    case IO_OBP1_ADDR:
      return TO_REG(e->oam.obp[1].color[3], PALETTE_COLOR3) |
             TO_REG(e->oam.obp[1].color[2], PALETTE_COLOR2) |
             TO_REG(e->oam.obp[1].color[1], PALETTE_COLOR1);
    case IO_WY_ADDR:
      return e->lcd.WY;
    case IO_WX_ADDR:
      return e->lcd.WX; /* TODO actually represents window X minus 7 */
    case IO_IE_ADDR:
      return e->interrupts.IE;
    default:
      LOG("read_io(0x%04x [%s]) ignored.\n", addr, get_io_reg_string(addr));
      return 0;
  }
}

uint8_t read_u8(struct Emulator* e, Address addr) {
  struct MemoryTypeAddressPair pair = map_address(addr);
  if (e->dma.active && pair.type != MEMORY_MAP_HIGH_RAM) {
    LOG("read_u8(0x%04x) during DMA.\n", addr);
    return 0;
  }

  switch (pair.type) {
    case MEMORY_MAP_ROM:
      return e->rom_data.data[pair.addr];

    case MEMORY_MAP_ROM_BANK_SWITCH:
      return e->memory_map.read_rom_bank_switch(e, pair.addr);

    case MEMORY_MAP_VRAM:
      return read_vram(e, pair.addr);

    case MEMORY_MAP_EXTERNAL_RAM:
      return e->memory_map.read_external_ram(e, pair.addr);

    case MEMORY_MAP_WORK_RAM:
      return e->ram.data[pair.addr];

    case MEMORY_MAP_WORK_RAM_BANK_SWITCH:
      return e->memory_map.read_work_ram_bank_switch(e, pair.addr);

    case MEMORY_MAP_OAM:
      return read_oam(e, pair.addr);

    case MEMORY_MAP_UNUSED:
      return 0;

    case MEMORY_MAP_IO: {
      uint8_t value = read_io(e, pair.addr);
      DEBUG("read_io(0x%04x) = 0x%02x\n", pair.addr, value);
      return value;
    }

    case MEMORY_MAP_HIGH_RAM:
      return e->hram[pair.addr];
  }
  UNREACHABLE("invalid address: 0x%04x.\n", addr);
}

uint16_t read_u16(struct Emulator* e, Address addr) {
  return read_u8(e, addr) | (read_u8(e, addr + 1) << 8);
}

void write_vram_tile_data(struct Emulator* e,
                          uint32_t index,
                          uint32_t plane,
                          uint32_t y,
                          uint8_t value) {
  DEBUG("write_vram_tile_data: [%u] (%u, %u) = %u\n", index, plane, y, value);
  assert(index < TILE_COUNT);
  Tile* tile = &e->vram.tile[index];
  uint32_t data_index = y * TILE_WIDTH;
  assert(data_index < TILE_WIDTH * TILE_HEIGHT);
  uint8_t* data = &(*tile)[data_index];
  uint8_t i;
  uint8_t mask = 1 << plane;
  uint8_t not_mask = ~mask;
  for (i = 0; i < 8; ++i) {
    data[i] = (data[i] & not_mask) | (((value >> (7 - i)) << plane) & mask);
  }
}

void write_vram(struct Emulator* e, MaskedAddress addr, uint8_t value) {
  /* Ignore writes if the display is on, and the hardware is using vram. */
  if (e->lcd.lcdc.display && e->lcd.stat.mode == LCD_MODE_USING_OAM_VRAM) {
    return;
  }

  assert(addr <= ADDR_MASK_8K);
  /* Store the raw data so it doesn't have to be re-packed when reading. */
  e->vram.data[addr] = value;

  if (addr < 0x1800) {
    /* 0x8000-0x97ff: Tile data */
    uint32_t tile_index = addr >> 4;     /* 16 bytes per tile. */
    uint32_t tile_y = (addr >> 1) & 0x7; /* 2 bytes per row. */
    uint32_t plane = addr & 1;
    write_vram_tile_data(e, tile_index, plane, tile_y, value);
    if (tile_index >= 128 && tile_index < 256) {
      /* Mirror tile data from 0x8800-0x8fff as if it were at 0x9800-0x9fff; it
       * isn't actually, but when using TILE_DATA_8800_97FF, the tile indexes
       * work as though it was. */
      write_vram_tile_data(e, tile_index + 256, plane, tile_y, value);
    }
  } else {
    /* 0x9800-0x9fff: Tile map data */
    addr -= 0x1800; /* Adjust to range 0x000-0x7ff. */
    uint32_t map_index = addr >> 10;
    assert(map_index < MAP_COUNT);
    TileMap* map = &e->vram.map[map_index];
    uint32_t map_y = (addr >> 5) & 0x1f; /* 32 bytes per row. */
    uint32_t map_x = addr & 0x1f;
    uint32_t map_data_index = map_y * TILE_MAP_WIDTH + map_x;
    assert(map_data_index < TILE_MAP_WIDTH * TILE_MAP_HEIGHT);
    uint8_t* data = &(*map)[map_data_index];
    *data = value;
  }
}

void write_oam(struct Emulator* e, MaskedAddress addr, uint8_t value) {
  /* Ignore writes if the display is on, and the hardware is using OAM. */
  if (e->lcd.lcdc.display && (e->lcd.stat.mode == LCD_MODE_USING_OAM ||
                              e->lcd.stat.mode == LCD_MODE_USING_OAM_VRAM)) {
    return;
  }

  uint8_t obj_index = addr >> 2;
  struct Obj* obj = &e->oam.objs[obj_index];
  switch (addr & 3) {
    case 0:
      obj->y = value - OBJ_Y_OFFSET;
      break;

    case 1:
      obj->x = value - OBJ_X_OFFSET;
      break;

    case 2:
      obj->tile = value;
      break;

    case 3:
      obj->priority = TO_REG(value, OBJ_PRIORITY);
      obj->yflip = TO_REG(value, OBJ_YFLIP);
      obj->xflip = TO_REG(value, OBJ_XFLIP);
      obj->palette = TO_REG(value, OBJ_PALETTE);
      break;
  }
}

void from_nrx1_reg(struct Channel* channel, uint8_t value) {
  channel->wave_duty = FROM_REG(value, NRX1_WAVE_DUTY);
  channel->sound_length = FROM_REG(value, NRX1_SOUND_LENGTH);
}

void from_nrx2_reg(struct Channel* channel, uint8_t value) {
  channel->initial_volume = FROM_REG(value, NRX2_INITIAL_VOLUME);
  channel->envelope_direction = FROM_REG(value, NRX2_ENVELOPE_DIRECTION);
  channel->envelope_count = FROM_REG(value, NRX2_ENVELOPE_COUNT);
}

void from_nrx3_reg(struct Channel* channel, uint8_t value) {
  channel->frequency &= ~0xff;
  channel->frequency |= value;
}

void from_nrx4_reg(struct Channel* channel, uint8_t value) {
  channel->initial = FROM_REG(value, NRX4_INITIAL);
  channel->consecutive = FROM_REG(value, NRX4_COUNTER_CONSECUTIVE);
  channel->frequency &= 0xff;
  channel->frequency |= FROM_REG(value, NRX4_FREQUENCY_HI) << 8;
}

void write_io(struct Emulator* e, MaskedAddress addr, uint8_t value) {
  DEBUG("write_io(0x%04x [%s], %u)\n", addr, get_io_reg_string(addr), value);
  switch (addr) {
    case IO_JOYP_ADDR:
      e->joypad.joypad_select = FROM_REG(value, JOYP_JOYPAD_SELECT);
      break;
    case IO_SB_ADDR: /* TODO */
      break;
    case IO_SC_ADDR:
      e->serial.transfer_start = FROM_REG(value, SC_TRANSFER_START);
      e->serial.clock_speed = FROM_REG(value, SC_CLOCK_SPEED);
      e->serial.shift_clock = FROM_REG(value, SC_SHIFT_CLOCK);
      break;
    case IO_DIV_ADDR:
      e->timer.DIV = 0;
      break;
    case IO_TIMA_ADDR:
      e->timer.TIMA = value; /* TODO is this correct? */
      break;
    case IO_TMA_ADDR:
      e->timer.TMA = value;
      break;
    case IO_TAC_ADDR:
      e->timer.input_clock_select = FROM_REG(value, TAC_INPUT_CLOCK_SELECT);
      e->timer.on = FROM_REG(value, TAC_TIMER_ON);
      break;
    case IO_NR10_ADDR: {
      struct Channel* channel = &e->sound.channel[CHANNEL1];
      channel->sweep_time = FROM_REG(value, NR10_SWEEP_TIME);
      channel->sweep_direction = FROM_REG(value, NR10_SWEEP_DIRECTION);
      channel->sweep_count = FROM_REG(value, NR10_SWEEP_COUNT);
      break;
    }
    case IO_NR11_ADDR:
      from_nrx1_reg(&e->sound.channel[CHANNEL1], value);
      break;
    case IO_NR12_ADDR:
      from_nrx2_reg(&e->sound.channel[CHANNEL1], value);
      break;
    case IO_NR13_ADDR:
      from_nrx3_reg(&e->sound.channel[CHANNEL1], value);
      break;
    case IO_NR14_ADDR:
      from_nrx4_reg(&e->sound.channel[CHANNEL1], value);
      break;
    case IO_NR21_ADDR:
      from_nrx1_reg(&e->sound.channel[CHANNEL2], value);
      break;
    case IO_NR22_ADDR:
      from_nrx2_reg(&e->sound.channel[CHANNEL2], value);
      break;
    case IO_NR23_ADDR:
      from_nrx3_reg(&e->sound.channel[CHANNEL2], value);
      break;
    case IO_NR24_ADDR:
      from_nrx4_reg(&e->sound.channel[CHANNEL2], value);
      break;
    case IO_NR30_ADDR:
      e->sound.channel[CHANNEL3].on = FROM_REG(value, NR30_SOUND_ON);
      break;
    case IO_NR31_ADDR:
      e->sound.channel[CHANNEL3].sound_length = value;
      break;
    case IO_NR32_ADDR:
      e->sound.channel[CHANNEL3].output_level =
          FROM_REG(value, NR32_SELECT_OUTPUT_LEVEL);
      break;
    case IO_NR33_ADDR:
      from_nrx3_reg(&e->sound.channel[CHANNEL3], value);
      break;
    case IO_NR34_ADDR:
      from_nrx4_reg(&e->sound.channel[CHANNEL3], value);
      break;
    case IO_NR41_ADDR:
      from_nrx1_reg(&e->sound.channel[CHANNEL4], value);
      break;
    case IO_NR42_ADDR:
      from_nrx2_reg(&e->sound.channel[CHANNEL4], value);
      break;
    case IO_NR43_ADDR: {
      struct Channel* channel = &e->sound.channel[CHANNEL4];
      channel->shift_clock_frequency =
          FROM_REG(value, NR43_SHIFT_CLOCK_FREQUENCY);
      channel->counter_step = FROM_REG(value, NR43_COUNTER_STEP);
      channel->divide_ratio = FROM_REG(value, NR43_DIVIDE_RATIO);
      break;
    }
    case IO_NR44_ADDR:
      from_nrx4_reg(&e->sound.channel[CHANNEL4], value);
      break;
    case IO_NR50_ADDR:
      e->sound.so2_output[VIN] = FROM_REG(value, NR50_VIN_SO2);
      e->sound.so2_volume = FROM_REG(value, NR50_SO2_VOLUME);
      e->sound.so1_output[VIN] = FROM_REG(value, NR50_VIN_SO1);
      e->sound.so1_volume = FROM_REG(value, NR50_SO1_VOLUME);
      break;
    case IO_NR51_ADDR:
      e->sound.so2_output[SOUND4] = FROM_REG(value, NR51_SOUND4_SO2);
      e->sound.so2_output[SOUND3] = FROM_REG(value, NR51_SOUND3_SO2);
      e->sound.so2_output[SOUND2] = FROM_REG(value, NR51_SOUND2_SO2);
      e->sound.so2_output[SOUND1] = FROM_REG(value, NR51_SOUND1_SO2);
      e->sound.so1_output[SOUND4] = FROM_REG(value, NR51_SOUND4_SO1);
      e->sound.so1_output[SOUND3] = FROM_REG(value, NR51_SOUND3_SO1);
      e->sound.so1_output[SOUND2] = FROM_REG(value, NR51_SOUND2_SO1);
      e->sound.so1_output[SOUND1] = FROM_REG(value, NR51_SOUND1_SO1);
      break;
    case IO_NR52_ADDR:
      e->sound.enabled = FROM_REG(value, NR52_ALL_SOUND_ENABLED);
      break;
    case IO_IF_ADDR:
      e->interrupts.IF = value;
      break;
    case IO_LCDC_ADDR:
      e->lcd.lcdc.display = FROM_REG(value, LCDC_DISPLAY);
      e->lcd.lcdc.window_tile_map_select =
          FROM_REG(value, LCDC_WINDOW_TILE_MAP_SELECT);
      e->lcd.lcdc.window_display = FROM_REG(value, LCDC_WINDOW_DISPLAY);
      e->lcd.lcdc.bg_tile_data_select =
          FROM_REG(value, LCDC_BG_TILE_DATA_SELECT);
      e->lcd.lcdc.bg_tile_map_select = FROM_REG(value, LCDC_BG_TILE_MAP_SELECT);
      e->lcd.lcdc.obj_size = FROM_REG(value, LCDC_OBJ_SIZE);
      e->lcd.lcdc.obj_display = FROM_REG(value, LCDC_OBJ_DISPLAY);
      e->lcd.lcdc.bg_display = FROM_REG(value, LCDC_BG_DISPLAY);
      break;
    case IO_STAT_ADDR:
      e->lcd.stat.y_compare_intr = FROM_REG(value, STAT_YCOMPARE_INTR);
      e->lcd.stat.using_oam_intr = FROM_REG(value, STAT_USING_OAM_INTR);
      e->lcd.stat.vblank_intr = FROM_REG(value, STAT_VBLANK_INTR);
      e->lcd.stat.hblank_intr = FROM_REG(value, STAT_HBLANK_INTR);
      break;
    case IO_SCY_ADDR:
      e->lcd.SCY = value;
      break;
    case IO_SCX_ADDR:
      e->lcd.SCX = value;
      break;
    case IO_LY_ADDR:
      break;
    case IO_LYC_ADDR:
      e->lcd.LYC = value;
      break;
    case IO_DMA_ADDR:
      e->dma.active = TRUE;
      e->dma.addr = value << 8;
      e->dma.cycles = 0;
      break;
    case IO_BGP_ADDR:
      e->lcd.bgp.color[3] = FROM_REG(value, PALETTE_COLOR3);
      e->lcd.bgp.color[2] = FROM_REG(value, PALETTE_COLOR2);
      e->lcd.bgp.color[1] = FROM_REG(value, PALETTE_COLOR1);
      e->lcd.bgp.color[0] = FROM_REG(value, PALETTE_COLOR0);
      break;
    case IO_OBP0_ADDR:
      e->oam.obp[0].color[3] = FROM_REG(value, PALETTE_COLOR3);
      e->oam.obp[0].color[2] = FROM_REG(value, PALETTE_COLOR2);
      e->oam.obp[0].color[1] = FROM_REG(value, PALETTE_COLOR1);
      break;
    case IO_OBP1_ADDR:
      e->oam.obp[1].color[3] = FROM_REG(value, PALETTE_COLOR3);
      e->oam.obp[1].color[2] = FROM_REG(value, PALETTE_COLOR2);
      e->oam.obp[1].color[1] = FROM_REG(value, PALETTE_COLOR1);
      break;
    case IO_WY_ADDR:
      e->lcd.WY = value;
      break;
    case IO_WX_ADDR:
      e->lcd.WX = value;
      break;
    case IO_IE_ADDR:
      e->interrupts.IE = value;
      break;
    default:
      LOG("write_io(0x%04x, %u) ignored.\n", addr, value);
      break;
  }
}

void write_u8(struct Emulator* e, Address addr, uint8_t value) {
  struct MemoryTypeAddressPair pair = map_address(addr);
  if (e->dma.active && pair.type != MEMORY_MAP_HIGH_RAM) {
    LOG("write_u8(0x%04x, 0x%02x) during DMA.\n", addr, value);
    return;
  }

  switch (pair.type) {
    case MEMORY_MAP_ROM:
      e->memory_map.write_rom(e, pair.addr, value);
      break;

    case MEMORY_MAP_ROM_BANK_SWITCH:
      /* TODO(binji): cleanup */
      e->memory_map.write_rom(e, pair.addr + 0x4000, value);
      break;

    case MEMORY_MAP_VRAM:
      return write_vram(e, pair.addr, value);

    case MEMORY_MAP_EXTERNAL_RAM:
      e->memory_map.write_external_ram(e, pair.addr, value);
      break;

    case MEMORY_MAP_WORK_RAM:
      e->ram.data[pair.addr] = value;
      break;

    case MEMORY_MAP_WORK_RAM_BANK_SWITCH:
      e->memory_map.write_work_ram_bank_switch(e, pair.addr, value);
      break;

    case MEMORY_MAP_OAM:
      write_oam(e, pair.addr, value);
      break;

    case MEMORY_MAP_UNUSED:
      break;

    case MEMORY_MAP_IO:
      return write_io(e, pair.addr, value);

    case MEMORY_MAP_HIGH_RAM:
      DEBUG("write_hram(0x%04x, 0x%02x)\n", addr, value);
      e->hram[pair.addr] = value;
      break;
  }
}

void write_u16(struct Emulator* e, Address addr, uint16_t value) {
  write_u8(e, addr, value);
  write_u8(e, addr + 1, value >> 8);
}

TileMap* get_tile_map(struct Emulator* e, enum TileMapSelect select) {
  switch (select) {
    case TILE_MAP_9800_9BFF: return &e->vram.map[0];
    case TILE_MAP_9C00_9FFF: return &e->vram.map[1];
    default: return NULL;
  }
}

Tile* get_tile_data(struct Emulator* e, enum TileDataSelect select) {
  switch (select) {
    case TILE_DATA_8000_8FFF: return &e->vram.tile[0];
    case TILE_DATA_8800_97FF: return &e->vram.tile[256];
    default: return NULL;
  }
}

uint8_t get_tile_map_palette_index(TileMap* map,
                                   Tile* tiles,
                                   uint8_t x,
                                   uint8_t y) {
  uint8_t tile_index = (*map)[((y >> 3) * TILE_MAP_WIDTH) | (x >> 3)];
  Tile* tile = &tiles[tile_index];
  return (*tile)[(y & 7) * TILE_WIDTH | (x & 7)];
}

RGBA get_palette_index_rgba(uint8_t palette_index, struct Palette* palette) {
  return s_color_to_rgba[palette->color[palette_index]];
}

int obj_cmp(const void* v1, const void* v2, void* arg) {
  const struct Obj* o1 = v1;
  const struct Obj* o2 = v2;
  const uint8_t* obj_height = arg;
  enum Bool o1_visible = o1->y < *obj_height;
  enum Bool o2_visible = o2->y < *obj_height;
  if (o1_visible != o2_visible) {
    /* Put invisible sprites at the end. */
    return o2_visible - o1_visible;
  }
  if (o1->x != o2->x) {
    /* Prioritize sprites with a smaller X value. */
    return o1->x - o2->x;
  }
  /* Otherwise use table order (i.e. pointer value). */
  return o1 - o2;
}

void render_line(struct Emulator* e) {
  uint8_t line_y = e->lcd.LY;
  assert(line_y < SCREEN_HEIGHT);
  RGBA* line_data = &e->frame_buffer[line_y * SCREEN_WIDTH];

  uint8_t bg_obj_mask[SCREEN_WIDTH];

  if (!e->lcd.lcdc.display || !e->lcd.lcdc.bg_display) {
    uint8_t sx;
    for (sx = 0; sx < SCREEN_WIDTH; ++sx) {
      bg_obj_mask[sx] = s_color_to_obj_mask[COLOR_WHITE];
      line_data[sx] = RGBA_WHITE;
    }

    if (!e->lcd.lcdc.display) {
      return;
    }
  }

  if (e->lcd.lcdc.bg_display) {
    TileMap* map = get_tile_map(e, e->lcd.lcdc.bg_tile_map_select);
    Tile* tiles = get_tile_data(e, e->lcd.lcdc.bg_tile_data_select);
    struct Palette* palette = &e->lcd.bgp;
    uint8_t bg_y = line_y + e->lcd.SCY;
    uint8_t bg_x = e->lcd.SCX;
    int sx;
    for (sx = 0; sx < SCREEN_WIDTH; ++sx, ++bg_x) {
      uint8_t palette_index =
          get_tile_map_palette_index(map, tiles, bg_x, bg_y);
      bg_obj_mask[sx] = s_color_to_obj_mask[palette_index];
      line_data[sx] = get_palette_index_rgba(palette_index, palette);
    }
  }

  if (e->lcd.lcdc.window_display && e->lcd.WX <= WINDOW_MAX_X &&
      e->lcd.frame_WY <= WINDOW_MAX_Y && e->lcd.win_y >= e->lcd.frame_WY) {
    TileMap* map = get_tile_map(e, e->lcd.lcdc.window_tile_map_select);
    Tile* tiles = get_tile_data(e, e->lcd.lcdc.bg_tile_data_select);
    struct Palette* palette = &e->lcd.bgp;
    uint8_t win_x = 0;
    int sx = 0;
    if (e->lcd.WX < WINDOW_X_OFFSET) {
      /* Start at the leftmost screen X, but skip N pixels of the window. */
      win_x = WINDOW_X_OFFSET - e->lcd.WX;
    } else {
      /* Start N pixels right of the left of the screen. */
      sx += e->lcd.WX - WINDOW_X_OFFSET;
    }
    for (; win_x < SCREEN_WIDTH; ++win_x) {
      uint8_t palette_index =
          get_tile_map_palette_index(map, tiles, win_x, e->lcd.win_y);
      bg_obj_mask[sx] = s_color_to_obj_mask[palette_index];
      line_data[sx] = get_palette_index_rgba(palette_index, palette);
    }
    e->lcd.win_y++;
  }

  if (e->lcd.lcdc.obj_display) {
    int n;
    struct Obj line_objs[OBJ_COUNT];
    memcpy(line_objs, e->oam.objs, sizeof(line_objs));
    uint8_t obj_height = s_obj_size_to_height[e->lcd.lcdc.obj_size];
    for (n = 0; n < OBJ_COUNT; ++n) {
      line_objs[n].y = line_y - line_objs[n].y;
    }

    qsort_r(line_objs, OBJ_COUNT, sizeof(struct Obj), obj_cmp, &obj_height);
    /* Draw in reverse so sprites with higher priority are rendered on top. */
    for (n = OBJ_PER_LINE_COUNT - 1; n >= 0; --n) {
      struct Obj* o = &line_objs[n];
      uint8_t oy = o->y;
      if (oy >= obj_height) {
        continue;
      }

      uint8_t* tile_data;
      if (o->yflip) {
        oy = obj_height - oy;
      }
      if (obj_height == 8) {
        tile_data = &e->vram.tile[o->tile][oy * TILE_HEIGHT];
      } else if (oy < 8) {
        /* Top tile of 8x16 sprite. */
        tile_data = &e->vram.tile[o->tile & 0xfe][oy * TILE_HEIGHT];
      } else {
        /* Bottom tile of 8x16 sprite. */
        tile_data = &e->vram.tile[o->tile | 0x01][(oy - 8) * TILE_HEIGHT];
      }

      struct Palette* palette = &e->oam.obp[o->palette];
      int n;
      int d = o->xflip ? -1: 1;
      int ox = o->xflip ? 7 : 0;
      for (n = 0; n < 8; ++n, ox += d) {
        uint8_t sx = o->x + n;
        if (sx >= SCREEN_WIDTH) {
          continue;
        }
        if (o->priority == OBJ_PRIORITY_BEHIND_BG && bg_obj_mask[sx] == 0) {
          continue;
        }
        uint8_t palette_index = tile_data[ox];
        line_data[sx] = get_palette_index_rgba(palette_index, palette);
      }
    }
  }
}

uint8_t s_opcode_bytes[] = {
    /*       0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f */
    /* 00 */ 1, 3, 1, 1, 1, 1, 2, 1, 3, 1, 1, 1, 1, 1, 2, 1,
    /* 10 */ 1, 3, 1, 1, 1, 1, 2, 1, 2, 1, 1, 1, 1, 1, 2, 1,
    /* 20 */ 2, 3, 1, 1, 1, 1, 2, 1, 2, 1, 1, 1, 1, 1, 2, 1,
    /* 30 */ 2, 3, 1, 1, 1, 1, 2, 1, 2, 1, 1, 1, 1, 1, 2, 1,
    /* 40 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* 50 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* 60 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* 70 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* 80 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* 90 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* a0 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* b0 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* c0 */ 1, 1, 3, 3, 3, 1, 2, 1, 1, 1, 3, 2, 3, 3, 2, 1,
    /* d0 */ 1, 1, 3, 0, 3, 1, 2, 1, 1, 1, 3, 0, 3, 0, 2, 1,
    /* e0 */ 2, 1, 1, 0, 0, 1, 2, 1, 2, 1, 3, 0, 0, 0, 2, 1,
    /* f0 */ 2, 1, 1, 1, 0, 1, 2, 1, 2, 1, 3, 1, 0, 0, 2, 1,
};

const char* s_opcode_mnemonic[] = {
        [0x00] = "NOP",
        [0x01] = "LD BC,%hu",
        [0x02] = "LD (BC),A",
        [0x03] = "INC BC",
        [0x04] = "INC B",
        [0x05] = "DEC B",
        [0x06] = "LD B,%hhu",
        [0x07] = "RLCA",
        [0x08] = "LD (%04hXH),SP",
        [0x09] = "ADD HL,BC",
        [0x0A] = "LD A,(BC)",
        [0x0B] = "DEC BC",
        [0x0C] = "INC C",
        [0x0D] = "DEC C",
        [0x0E] = "LD C,%hhu",
        [0x0F] = "RRCA",
        [0x10] = "STOP",
        [0x11] = "LD DE,%hu",
        [0x12] = "LD (DE),A",
        [0x13] = "INC DE",
        [0x14] = "INC D",
        [0x15] = "DEC D",
        [0x16] = "LD D,%hhu",
        [0x17] = "RLA",
        [0x18] = "JR %+hhd",
        [0x19] = "ADD HL,DE",
        [0x1A] = "LD A,(DE)",
        [0x1B] = "DEC DE",
        [0x1C] = "INC E",
        [0x1D] = "DEC E",
        [0x1E] = "LD E,%hhu",
        [0x1F] = "RRA",
        [0x20] = "JR NZ,%+hhd",
        [0x21] = "LD HL,%hu",
        [0x22] = "LDI (HL),A",
        [0x23] = "INC HL",
        [0x24] = "INC H",
        [0x25] = "DEC H",
        [0x26] = "LD H,%hhu",
        [0x27] = "DAA",
        [0x28] = "JR Z,%+hhd",
        [0x29] = "ADD HL,HL",
        [0x2A] = "LDI A,(HL)",
        [0x2B] = "DEC HL",
        [0x2C] = "INC L",
        [0x2D] = "DEC L",
        [0x2E] = "LD L,%hhu",
        [0x2F] = "CPL",
        [0x30] = "JR NC,%+hhd",
        [0x31] = "LD SP,%hu",
        [0x32] = "LDD (HL),A",
        [0x33] = "INC SP",
        [0x34] = "INC (HL)",
        [0x35] = "DEC (HL)",
        [0x36] = "LD (HL),%hhu",
        [0x37] = "SCF",
        [0x38] = "JR C,%+hhd",
        [0x39] = "ADD HL,SP",
        [0x3A] = "LDD A,(HL)",
        [0x3B] = "DEC SP",
        [0x3C] = "INC A",
        [0x3D] = "DEC A",
        [0x3E] = "LD A,%hhu",
        [0x3F] = "CCF",
        [0x40] = "LD B,B",
        [0x41] = "LD B,C",
        [0x42] = "LD B,D",
        [0x43] = "LD B,E",
        [0x44] = "LD B,H",
        [0x45] = "LD B,L",
        [0x46] = "LD B,(HL)",
        [0x47] = "LD B,A",
        [0x48] = "LD C,B",
        [0x49] = "LD C,C",
        [0x4A] = "LD C,D",
        [0x4B] = "LD C,E",
        [0x4C] = "LD C,H",
        [0x4D] = "LD C,L",
        [0x4E] = "LD C,(HL)",
        [0x4F] = "LD C,A",
        [0x50] = "LD D,B",
        [0x51] = "LD D,C",
        [0x52] = "LD D,D",
        [0x53] = "LD D,E",
        [0x54] = "LD D,H",
        [0x55] = "LD D,L",
        [0x56] = "LD D,(HL)",
        [0x57] = "LD D,A",
        [0x58] = "LD E,B",
        [0x59] = "LD E,C",
        [0x5A] = "LD E,D",
        [0x5B] = "LD E,E",
        [0x5C] = "LD E,H",
        [0x5D] = "LD E,L",
        [0x5E] = "LD E,(HL)",
        [0x5F] = "LD E,A",
        [0x60] = "LD H,B",
        [0x61] = "LD H,C",
        [0x62] = "LD H,D",
        [0x63] = "LD H,E",
        [0x64] = "LD H,H",
        [0x65] = "LD H,L",
        [0x66] = "LD H,(HL)",
        [0x67] = "LD H,A",
        [0x68] = "LD L,B",
        [0x69] = "LD L,C",
        [0x6A] = "LD L,D",
        [0x6B] = "LD L,E",
        [0x6C] = "LD L,H",
        [0x6D] = "LD L,L",
        [0x6E] = "LD L,(HL)",
        [0x6F] = "LD L,A",
        [0x70] = "LD (HL),B",
        [0x71] = "LD (HL),C",
        [0x72] = "LD (HL),D",
        [0x73] = "LD (HL),E",
        [0x74] = "LD (HL),H",
        [0x75] = "LD (HL),L",
        [0x76] = "HALT",
        [0x77] = "LD (HL),A",
        [0x78] = "LD A,B",
        [0x79] = "LD A,C",
        [0x7A] = "LD A,D",
        [0x7B] = "LD A,E",
        [0x7C] = "LD A,H",
        [0x7D] = "LD A,L",
        [0x7E] = "LD A,(HL)",
        [0x7F] = "LD A,A",
        [0x80] = "ADD A,B",
        [0x81] = "ADD A,C",
        [0x82] = "ADD A,D",
        [0x83] = "ADD A,E",
        [0x84] = "ADD A,H",
        [0x85] = "ADD A,L",
        [0x86] = "ADD A,(HL)",
        [0x87] = "ADD A,A",
        [0x88] = "ADC A,B",
        [0x89] = "ADC A,C",
        [0x8A] = "ADC A,D",
        [0x8B] = "ADC A,E",
        [0x8C] = "ADC A,H",
        [0x8D] = "ADC A,L",
        [0x8E] = "ADC A,(HL)",
        [0x8F] = "ADC A,A",
        [0x90] = "SUB B",
        [0x91] = "SUB C",
        [0x92] = "SUB D",
        [0x93] = "SUB E",
        [0x94] = "SUB H",
        [0x95] = "SUB L",
        [0x96] = "SUB (HL)",
        [0x97] = "SUB A",
        [0x98] = "SBC B",
        [0x99] = "SBC C",
        [0x9A] = "SBC D",
        [0x9B] = "SBC E",
        [0x9C] = "SBC H",
        [0x9D] = "SBC L",
        [0x9E] = "SBC (HL)",
        [0x9F] = "SBC A",
        [0xA0] = "AND B",
        [0xA1] = "AND C",
        [0xA2] = "AND D",
        [0xA3] = "AND E",
        [0xA4] = "AND H",
        [0xA5] = "AND L",
        [0xA6] = "AND (HL)",
        [0xA7] = "AND A",
        [0xA8] = "XOR B",
        [0xA9] = "XOR C",
        [0xAA] = "XOR D",
        [0xAB] = "XOR E",
        [0xAC] = "XOR H",
        [0xAD] = "XOR L",
        [0xAE] = "XOR (HL)",
        [0xAF] = "XOR A",
        [0xB0] = "OR B",
        [0xB1] = "OR C",
        [0xB2] = "OR D",
        [0xB3] = "OR E",
        [0xB4] = "OR H",
        [0xB5] = "OR L",
        [0xB6] = "OR (HL)",
        [0xB7] = "OR A",
        [0xB8] = "CP B",
        [0xB9] = "CP C",
        [0xBA] = "CP D",
        [0xBB] = "CP E",
        [0xBC] = "CP H",
        [0xBD] = "CP L",
        [0xBE] = "CP (HL)",
        [0xBF] = "CP A",
        [0xC0] = "RET NZ",
        [0xC1] = "POP BC",
        [0xC2] = "JP NZ,%04hXH",
        [0xC3] = "JP %04hXH",
        [0xC4] = "CALL NZ,%04hXH",
        [0xC5] = "PUSH BC",
        [0xC6] = "ADD A,%hhu",
        [0xC7] = "RST 0",
        [0xC8] = "RET Z",
        [0xC9] = "RET",
        [0xCA] = "JP Z,%04hXH",
        [0xCC] = "CALL Z,%04hXH",
        [0xCD] = "CALL %04hXH",
        [0xCE] = "ADC A,%hhu",
        [0xCF] = "RST 8H",
        [0xD0] = "RET NC",
        [0xD1] = "POP DE",
        [0xD2] = "JP NC,%04hXH",
        [0xD4] = "CALL NC,%04hXH",
        [0xD5] = "PUSH DE",
        [0xD6] = "SUB %hhu",
        [0xD7] = "RST 10H",
        [0xD8] = "RET C",
        [0xD9] = "RETI",
        [0xDA] = "JP C,%04hXH",
        [0xDC] = "CALL C,%04hXH",
        [0xDE] = "SBC A,%hhu",
        [0xDF] = "RST 18H",
        [0xE0] = "LD (FF%02hhXH),A",
        [0xE1] = "POP HL",
        [0xE2] = "LD (FF00H+C),A",
        [0xE5] = "PUSH HL",
        [0xE6] = "AND %hhu",
        [0xE7] = "RST 20H",
        [0xE8] = "ADD SP,%hhd",
        [0xE9] = "JP HL",
        [0xEA] = "LD (%04hXH),A",
        [0xEE] = "XOR %hhu",
        [0xEF] = "RST 28H",
        [0xF0] = "LD A,(FF%02hhXH)",
        [0xF1] = "POP AF",
        [0xF2] = "LD A,(FF00H+C)",
        [0xF3] = "DI",
        [0xF5] = "PUSH AF",
        [0xF6] = "OR %hhu",
        [0xF7] = "RST 30H",
        [0xF8] = "LD HL,SP%+hhd",
        [0xF9] = "LD SP,HL",
        [0xFA] = "LD A,(%04hXH)",
        [0xFB] = "EI",
        [0xFE] = "CP %hhu",
        [0xFF] = "RST 38H",
};

const char* s_cb_opcode_mnemonic[] = {
        [0x00] = "RLC B",      [0x01] = "RLC C",      [0x02] = "RLC D",
        [0x03] = "RLC E",      [0x04] = "RLC H",      [0x05] = "RLC L",
        [0x06] = "RLC (HL)",   [0x07] = "RLC A",      [0x08] = "RRC B",
        [0x09] = "RRC C",      [0x0A] = "RRC D",      [0x0B] = "RRC E",
        [0x0C] = "RRC H",      [0x0D] = "RRC L",      [0x0E] = "RRC (HL)",
        [0x0F] = "RRC A",      [0x10] = "RL B",       [0x11] = "RL C",
        [0x12] = "RL D",       [0x13] = "RL E",       [0x14] = "RL H",
        [0x15] = "RL L",       [0x16] = "RL (HL)",    [0x17] = "RL A",
        [0x18] = "RR B",       [0x19] = "RR C",       [0x1A] = "RR D",
        [0x1B] = "RR E",       [0x1C] = "RR H",       [0x1D] = "RR L",
        [0x1E] = "RR (HL)",    [0x1F] = "RR A",       [0x20] = "SLA B",
        [0x21] = "SLA C",      [0x22] = "SLA D",      [0x23] = "SLA E",
        [0x24] = "SLA H",      [0x25] = "SLA L",      [0x26] = "SLA (HL)",
        [0x27] = "SLA A",      [0x28] = "SRA B",      [0x29] = "SRA C",
        [0x2A] = "SRA D",      [0x2B] = "SRA E",      [0x2C] = "SRA H",
        [0x2D] = "SRA L",      [0x2E] = "SRA (HL)",   [0x2F] = "SRA A",
        [0x30] = "SWAP B",     [0x31] = "SWAP C",     [0x32] = "SWAP D",
        [0x33] = "SWAP E",     [0x34] = "SWAP H",     [0x35] = "SWAP L",
        [0x36] = "SWAP (HL)",  [0x37] = "SWAP A",     [0x38] = "SRL B",
        [0x39] = "SRL C",      [0x3A] = "SRL D",      [0x3B] = "SRL E",
        [0x3C] = "SRL H",      [0x3D] = "SRL L",      [0x3E] = "SRL (HL)",
        [0x3F] = "SRL A",      [0x40] = "BIT 0,B",    [0x41] = "BIT 0,C",
        [0x42] = "BIT 0,D",    [0x43] = "BIT 0,E",    [0x44] = "BIT 0,H",
        [0x45] = "BIT 0,L",    [0x46] = "BIT 0,(HL)", [0x47] = "BIT 0,A",
        [0x48] = "BIT 1,B",    [0x49] = "BIT 1,C",    [0x4A] = "BIT 1,D",
        [0x4B] = "BIT 1,E",    [0x4C] = "BIT 1,H",    [0x4D] = "BIT 1,L",
        [0x4E] = "BIT 1,(HL)", [0x4F] = "BIT 1,A",    [0x50] = "BIT 2,B",
        [0x51] = "BIT 2,C",    [0x52] = "BIT 2,D",    [0x53] = "BIT 2,E",
        [0x54] = "BIT 2,H",    [0x55] = "BIT 2,L",    [0x56] = "BIT 2,(HL)",
        [0x57] = "BIT 2,A",    [0x58] = "BIT 3,B",    [0x59] = "BIT 3,C",
        [0x5A] = "BIT 3,D",    [0x5B] = "BIT 3,E",    [0x5C] = "BIT 3,H",
        [0x5D] = "BIT 3,L",    [0x5E] = "BIT 3,(HL)", [0x5F] = "BIT 3,A",
        [0x60] = "BIT 4,B",    [0x61] = "BIT 4,C",    [0x62] = "BIT 4,D",
        [0x63] = "BIT 4,E",    [0x64] = "BIT 4,H",    [0x65] = "BIT 4,L",
        [0x66] = "BIT 4,(HL)", [0x67] = "BIT 4,A",    [0x68] = "BIT 5,B",
        [0x69] = "BIT 5,C",    [0x6A] = "BIT 5,D",    [0x6B] = "BIT 5,E",
        [0x6C] = "BIT 5,H",    [0x6D] = "BIT 5,L",    [0x6E] = "BIT 5,(HL)",
        [0x6F] = "BIT 5,A",    [0x70] = "BIT 6,B",    [0x71] = "BIT 6,C",
        [0x72] = "BIT 6,D",    [0x73] = "BIT 6,E",    [0x74] = "BIT 6,H",
        [0x75] = "BIT 6,L",    [0x76] = "BIT 6,(HL)", [0x77] = "BIT 6,A",
        [0x78] = "BIT 7,B",    [0x79] = "BIT 7,C",    [0x7A] = "BIT 7,D",
        [0x7B] = "BIT 7,E",    [0x7C] = "BIT 7,H",    [0x7D] = "BIT 7,L",
        [0x7E] = "BIT 7,(HL)", [0x7F] = "BIT 7,A",    [0x80] = "RES 0,B",
        [0x81] = "RES 0,C",    [0x82] = "RES 0,D",    [0x83] = "RES 0,E",
        [0x84] = "RES 0,H",    [0x85] = "RES 0,L",    [0x86] = "RES 0,(HL)",
        [0x87] = "RES 0,A",    [0x88] = "RES 1,B",    [0x89] = "RES 1,C",
        [0x8A] = "RES 1,D",    [0x8B] = "RES 1,E",    [0x8C] = "RES 1,H",
        [0x8D] = "RES 1,L",    [0x8E] = "RES 1,(HL)", [0x8F] = "RES 1,A",
        [0x90] = "RES 2,B",    [0x91] = "RES 2,C",    [0x92] = "RES 2,D",
        [0x93] = "RES 2,E",    [0x94] = "RES 2,H",    [0x95] = "RES 2,L",
        [0x96] = "RES 2,(HL)", [0x97] = "RES 2,A",    [0x98] = "RES 3,B",
        [0x99] = "RES 3,C",    [0x9A] = "RES 3,D",    [0x9B] = "RES 3,E",
        [0x9C] = "RES 3,H",    [0x9D] = "RES 3,L",    [0x9E] = "RES 3,(HL)",
        [0x9F] = "RES 3,A",    [0xA0] = "RES 4,B",    [0xA1] = "RES 4,C",
        [0xA2] = "RES 4,D",    [0xA3] = "RES 4,E",    [0xA4] = "RES 4,H",
        [0xA5] = "RES 4,L",    [0xA6] = "RES 4,(HL)", [0xA7] = "RES 4,A",
        [0xA8] = "RES 5,B",    [0xA9] = "RES 5,C",    [0xAA] = "RES 5,D",
        [0xAB] = "RES 5,E",    [0xAC] = "RES 5,H",    [0xAD] = "RES 5,L",
        [0xAE] = "RES 5,(HL)", [0xAF] = "RES 5,A",    [0xB0] = "RES 6,B",
        [0xB1] = "RES 6,C",    [0xB2] = "RES 6,D",    [0xB3] = "RES 6,E",
        [0xB4] = "RES 6,H",    [0xB5] = "RES 6,L",    [0xB6] = "RES 6,(HL)",
        [0xB7] = "RES 6,A",    [0xB8] = "RES 7,B",    [0xB9] = "RES 7,C",
        [0xBA] = "RES 7,D",    [0xBB] = "RES 7,E",    [0xBC] = "RES 7,H",
        [0xBD] = "RES 7,L",    [0xBE] = "RES 7,(HL)", [0xBF] = "RES 7,A",
        [0xC0] = "SET 0,B",    [0xC1] = "SET 0,C",    [0xC2] = "SET 0,D",
        [0xC3] = "SET 0,E",    [0xC4] = "SET 0,H",    [0xC5] = "SET 0,L",
        [0xC6] = "SET 0,(HL)", [0xC7] = "SET 0,A",    [0xC8] = "SET 1,B",
        [0xC9] = "SET 1,C",    [0xCA] = "SET 1,D",    [0xCB] = "SET 1,E",
        [0xCC] = "SET 1,H",    [0xCD] = "SET 1,L",    [0xCE] = "SET 1,(HL)",
        [0xCF] = "SET 1,A",    [0xD0] = "SET 2,B",    [0xD1] = "SET 2,C",
        [0xD2] = "SET 2,D",    [0xD3] = "SET 2,E",    [0xD4] = "SET 2,H",
        [0xD5] = "SET 2,L",    [0xD6] = "SET 2,(HL)", [0xD7] = "SET 2,A",
        [0xD8] = "SET 3,B",    [0xD9] = "SET 3,C",    [0xDA] = "SET 3,D",
        [0xDB] = "SET 3,E",    [0xDC] = "SET 3,H",    [0xDD] = "SET 3,L",
        [0xDE] = "SET 3,(HL)", [0xDF] = "SET 3,A",    [0xE0] = "SET 4,B",
        [0xE1] = "SET 4,C",    [0xE2] = "SET 4,D",    [0xE3] = "SET 4,E",
        [0xE4] = "SET 4,H",    [0xE5] = "SET 4,L",    [0xE6] = "SET 4,(HL)",
        [0xE7] = "SET 4,A",    [0xE8] = "SET 5,B",    [0xE9] = "SET 5,C",
        [0xEA] = "SET 5,D",    [0xEB] = "SET 5,E",    [0xEC] = "SET 5,H",
        [0xED] = "SET 5,L",    [0xEE] = "SET 5,(HL)", [0xEF] = "SET 5,A",
        [0xF0] = "SET 6,B",    [0xF1] = "SET 6,C",    [0xF2] = "SET 6,D",
        [0xF3] = "SET 6,E",    [0xF4] = "SET 6,H",    [0xF5] = "SET 6,L",
        [0xF6] = "SET 6,(HL)", [0xF7] = "SET 6,A",    [0xF8] = "SET 7,B",
        [0xF9] = "SET 7,C",    [0xFA] = "SET 7,D",    [0xFB] = "SET 7,E",
        [0xFC] = "SET 7,H",    [0xFD] = "SET 7,L",    [0xFE] = "SET 7,(HL)",
        [0xFF] = "SET 7,A",
};

void print_instruction(struct Emulator* e, Address addr) {
  uint8_t opcode = read_u8(e, addr);
  if (opcode == 0xcb) {
    uint8_t cb_opcode = read_u8(e, addr + 1);
    const char* mnemonic = s_cb_opcode_mnemonic[cb_opcode];
    printf("0x%04x: cb %02x     %-15s", addr, cb_opcode, mnemonic);
  } else {
    char buffer[64];
    const char* mnemonic = s_opcode_mnemonic[opcode];
    uint8_t bytes = s_opcode_bytes[opcode];
    switch (bytes) {
      case 0:
        printf("0x%04x: %02x        %-15s", addr, opcode, "*INVALID*");
        break;
      case 1:
        printf("0x%04x: %02x        %-15s", addr, opcode, mnemonic);
        break;
      case 2: {
        uint8_t byte = read_u8(e, addr + 1);
        snprintf(buffer, sizeof(buffer), mnemonic, byte);
        printf("0x%04x: %02x %02x     %-15s", addr, opcode, byte, buffer);
        break;
      }
      case 3: {
        uint8_t byte1 = read_u8(e, addr + 1);
        uint8_t byte2 = read_u8(e, addr + 2);
        uint16_t word = (byte2 << 8) | byte1;
        snprintf(buffer, sizeof(buffer), mnemonic, word);
        printf("0x%04x: %02x %02x %02x  %-15s", addr, opcode, byte1, byte2,
               buffer);
        break;
      }
      default:
        UNREACHABLE("invalid opcode byte length.\n");
        break;
    }
  }
}

void print_registers(struct Registers* reg) {
  printf("A:%02X F:%c%c%c%c BC:%04X DE:%04x HL:%04x SP:%04x PC:%04x", reg->A,
         reg->flags.Z ? 'Z' : '-', reg->flags.N ? 'N' : '-',
         reg->flags.H ? 'H' : '-', reg->flags.C ? 'C' : '-', reg->BC, reg->DE,
         reg->HL, reg->SP, reg->PC);
}

void print_emulator_info(struct Emulator* e) {
  if (s_trace) {
    print_registers(&e->reg);
    printf(" | ");
    print_instruction(e, e->reg.PC);
    printf("\n");
    if (s_trace_counter > 0) {
      if (--s_trace_counter == 0) {
        s_trace = FALSE;
      }
    }
  }
}

uint8_t s_opcode_cycles[] = {
  /* TODO(binji): guessed on 08H: LD (NN),SP; seems like it should be more
   * expensive than LD (NN),A which is 16 cycles. */
    /*        0   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f */
    /* 00 */  4, 12,  8,  8,  4,  4,  8,  4, 20,  8,  8,  8,  4,  4,  0,  4,
    /* 10 */  0, 12,  8,  8,  4,  4,  8,  4, 12,  8,  8,  8,  4,  4,  8,  4,
    /* 20 */  8, 12,  8,  8,  4,  4,  8,  4,  8,  8,  8,  8,  4,  4,  8,  4,
    /* 30 */  8, 12,  8,  8, 12, 12, 12,  4,  8,  8,  8,  8,  4,  4,  8,  4,
    /* 40 */  4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    /* 50 */  4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    /* 60 */  4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    /* 70 */  8,  8,  8,  8,  8,  8,  0,  8,  4,  4,  4,  4,  4,  4,  8,  4,
    /* 80 */  4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    /* 90 */  4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    /* a0 */  4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    /* b0 */  4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    /* c0 */  8, 12, 12, 16, 12, 16,  8, 16,  8, 16, 12,  0, 12, 24,  8, 16,
    /* d0 */  8, 12, 12,  0, 12, 16,  8, 16,  8, 16, 12,  0, 12,  0,  8, 16,
    /* e0 */ 12, 12,  8,  0,  0, 16,  8, 16, 16,  4, 16,  0,  0,  0,  8, 16,
    /* f0 */ 12, 12,  8,  4,  0, 16,  8, 16, 12,  8, 16,  4,  0,  0,  8, 16,
};

uint8_t s_cb_opcode_cycles[] = {
    /*        0   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f */
    /* 00 */  8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /* 10 */  8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /* 20 */  8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /* 30 */  8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /* 40 */  8,  8,  8,  8,  8,  8, 12,  8,  8,  8,  8,  8,  8,  8, 12,  8,
    /* 50 */  8,  8,  8,  8,  8,  8, 12,  8,  8,  8,  8,  8,  8,  8, 12,  8,
    /* 60 */  8,  8,  8,  8,  8,  8, 12,  8,  8,  8,  8,  8,  8,  8, 12,  8,
    /* 70 */  8,  8,  8,  8,  8,  8, 12,  8,  8,  8,  8,  8,  8,  8, 12,  8,
    /* 80 */  8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /* 90 */  8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /* a0 */  8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /* b0 */  8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /* c0 */  8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /* d0 */  8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /* e0 */  8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /* f0 */  8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
};

#define NI NOT_IMPLEMENTED("opcode not implemented!\n")
#define INVALID NI

#define REG(R) e->reg.R
#define FLAG(F) e->reg.flags.F
#define CYCLES(X) cycles += (X)

#define RA REG(A)
#define RHL REG(HL)
#define RSP REG(SP)
#define RPC REG(PC)
#define FZ FLAG(Z)
#define FC FLAG(C)
#define FH FLAG(H)
#define FN FLAG(N)

#define COND_NZ (!FZ)
#define COND_Z FZ
#define COND_NC (!FC)
#define COND_C FC
#define FZ_EQ0(X) FZ = (uint8_t)(X) == 0
#define FC_ADD(X, Y) FC = ((X) + (Y) > 0xff)
#define FC_ADD16(X, Y) FC = ((X) + (Y) > 0xffff)
#define FC_ADC(X, Y, C) FC = ((X) + (Y) + (C) > 0xff)
#define FC_SUB(X, Y) FC = ((int)(X) - (int)(Y) < 0)
#define FC_SBC(X, Y, C) FC = ((int)(X) - (int)(Y) - (int)(C) < 0)
#define MASK8(X) ((X) & 0xf)
#define MASK16(X) ((X) & 0xfff)
#define FH_ADD(X, Y) FH = (MASK8(X) + MASK8(Y) > 0xf)
#define FH_ADD16(X, Y) FH = (MASK16(X) + MASK16(Y) > 0xfff)
#define FH_ADC(X, Y, C) FH = (MASK8(X) + MASK8(Y) + C > 0xf)
#define FH_SUB(X, Y) FH = ((int)MASK8(X) - (int)MASK8(Y) < 0)
#define FH_SBC(X, Y, C) FH = ((int)MASK8(X) - (int)MASK8(Y) - (int)C < 0)
#define FCH_ADD(X, Y) FC_ADD(X, Y); FH_ADD(X, Y)
#define FCH_ADD16(X, Y) FC_ADD16(X, Y); FH_ADD16(X, Y)
#define FCH_ADC(X, Y, C) FC_ADC(X, Y, C); FH_ADC(X, Y, C)
#define FCH_SUB(X, Y) FC_SUB(X, Y); FH_SUB(X, Y)
#define FCH_SBC(X, Y, C) FC_SBC(X, Y, C); FH_SBC(X, Y, C)

#define READ8(X) read_u8(e, X)
#define READ16(X) read_u16(e, X)
#define WRITE8(X, V) write_u8(e, X, V)
#define WRITE16(X, V) write_u16(e, X, V)
#define READ_N READ8(RPC + 1)
#define READ_NN READ16(RPC + 1)
#define READMR(MR) READ8(REG(MR))
#define WRITEMR(MR, V) WRITE8(REG(MR), V)

#define BASIC_OP_R(R, OP) u = REG(R); OP; REG(R) = u
#define BASIC_OP_MR(MR, OP) u = READMR(MR); OP; WRITEMR(MR, u)

#define ADD_FLAGS(X, Y) FZ_EQ0((X) + (Y)); FN = 0; FCH_ADD(X, Y)
#define ADD_FLAGS16(X, Y) FN = 0; FCH_ADD16(X, Y)
#define ADD_SP_FLAGS(Y) FZ = FN = 0; FCH_ADD((uint8_t)RSP, (uint8_t)Y)
#define ADD_R(R) ADD_FLAGS(RA, REG(R)); RA += REG(R)
#define ADD_MR(MR) u = READMR(MR); ADD_FLAGS(RA, u); RA += u
#define ADD_N u = READ_N; ADD_FLAGS(RA, u); RA += u
#define ADD_HL_RR(RR) ADD_FLAGS16(RHL, REG(RR)); RHL += REG(RR)
#define ADD_SP_N s = (int8_t)READ_N; ADD_SP_FLAGS(s); RSP += s

#define ADC_FLAGS(X, Y, C) FZ_EQ0((X) + (Y) + (C)); FN = 0; FCH_ADC(X, Y, C)
#define ADC_R(R) u = REG(R); c = FC; ADC_FLAGS(RA, u, c); RA += u + c
#define ADC_MR(MR) u = READMR(MR); c = FC; ADC_FLAGS(RA, u, c); RA += u + c
#define ADC_N u = READ_N; c = FC; ADC_FLAGS(RA, u, c); RA += u + c

#define AND_FLAGS FZ_EQ0(RA); FH = 1; FN = FC = 0
#define AND_R(R) RA &= REG(R); AND_FLAGS
#define AND_MR(MR) RA &= READMR(MR); AND_FLAGS
#define AND_N RA &= READ_N; AND_FLAGS

#define BIT_FLAGS(BIT, X) FZ_EQ0(X & (1 << BIT)); FN = 0; FH = 1
#define BIT_R(BIT, R) u = REG(R); BIT_FLAGS(BIT, u)
#define BIT_MR(BIT, MR) u = READMR(MR); BIT_FLAGS(BIT, u)

#define CALL(X) RSP -= 2; WRITE16(RSP, new_pc); new_pc = X
#define CALL_NN CALL(READ_NN)
#define CALL_F_NN(COND) if (COND) { CALL_NN; CYCLES(12); }

#define CCF FC ^= 1; FN = FH = 0

#define CP_FLAGS(X, Y) FZ_EQ0(X - Y); FN = 1; FCH_SUB(X, Y)
#define CP_R(R) CP_FLAGS(RA, REG(R))
#define CP_N u = READ_N; CP_FLAGS(RA, u)
#define CP_MR(MR) u = READMR(MR); CP_FLAGS(RA, u)

#define CPL RA = ~RA; FN = FH = 1

#define DAA                              \
  do {                                   \
    u = 0;                               \
    if (FH || (!FN && (RA & 0xf) > 9)) { \
      u = 6;                             \
    }                                    \
    if (FC || (!FN && RA > 0x99)) {      \
      u |= 0x60;                         \
      FC = 1;                            \
    }                                    \
    RA += FN ? -u : u;                   \
    FZ_EQ0(RA);                          \
    FH = 0;                              \
  } while (0)

#define DEC u--
#define DEC_FLAGS FZ_EQ0(u); FN = 1; FH = MASK8(u) == 0xf
#define DEC_R(R) BASIC_OP_R(R, DEC); DEC_FLAGS
#define DEC_RR(RR) REG(RR)--
#define DEC_MR(MR) BASIC_OP_MR(MR, DEC); DEC_FLAGS

#define INC u++
#define INC_FLAGS FZ_EQ0(u); FN = 0; FH = MASK8(u) == 0
#define INC_R(R) BASIC_OP_R(R, INC); INC_FLAGS
#define INC_RR(RR) REG(RR)++
#define INC_MR(MR) BASIC_OP_MR(MR, INC); INC_FLAGS

#define JP_F_NN(COND) if (COND) { JP_NN; CYCLES(4); }
#define JP_RR(RR) new_pc = REG(RR)
#define JP_NN new_pc = READ_NN

#define JR_F_N(COND) if (COND) { JR_N; CYCLES(4); }
#define JR_N new_pc += (int8_t)READ_N

#define LD_R_R(RD, RS) REG(RD) = REG(RS)
#define LD_R_N(R) REG(R) = READ_N
#define LD_RR_RR(RRD, RRS) REG(RRD) = REG(RRS)
#define LD_RR_NN(RR) REG(RR) = READ_NN
#define LD_R_MR(R, MR) REG(R) = READMR(MR)
#define LD_R_MN(R) REG(R) = READ8(READ_NN)
#define LD_MR_R(MR, R) WRITEMR(MR, REG(R))
#define LD_MR_N(MR) WRITEMR(MR, READ_N)
#define LD_MN_R(R) WRITE8(READ_NN, REG(R))
#define LD_MFF00_N_R(R) WRITE8(0xFF00 + READ_N, RA)
#define LD_MFF00_R_R(R1, R2) WRITE8(0xFF00 + REG(R1), REG(R2))
#define LD_R_MFF00_N(R) REG(R) = READ8(0xFF00 + READ_N)
#define LD_R_MFF00_R(R1, R2) REG(R1) = READ8(0xFF00 + REG(R2))
#define LD_MNN_SP WRITE16(READ_NN, RSP)
#define LD_HL_SP_N s = (int8_t)READ_N; ADD_SP_FLAGS(s); RHL = RSP + s

#define OR_FLAGS FZ_EQ0(RA); FN = FH = FC = 0
#define OR_R(R) RA |= REG(R); OR_FLAGS
#define OR_MR(MR) RA |= READMR(MR); OR_FLAGS
#define OR_N RA |= READ_N; OR_FLAGS

#define POP_RR(RR) REG(RR) = READ16(RSP); RSP += 2
#define POP_AF set_af_reg(&e->reg, READ16(RSP)); RSP += 2

#define PUSH_RR(RR) RSP -= 2; WRITE16(RSP, REG(RR))
#define PUSH_AF RSP -= 2; WRITE16(RSP, get_af_reg(&e->reg))

#define RES(BIT) u &= ~(1 << (BIT))
#define RES_R(BIT, R) BASIC_OP_R(R, RES(BIT))
#define RES_MR(BIT, MR) BASIC_OP_MR(MR, RES(BIT))

#define RET new_pc = READ16(RSP); RSP += 2
#define RET_F(COND) if (COND) { RET; CYCLES(12); }
#define RETI e->interrupts.IME = TRUE; RET

#define RL c = (u >> 7) & 1; u = (u << 1) | FC; FC = c
#define RL_FLAGS FZ_EQ0(u); FN = FH = 0
#define RLA BASIC_OP_R(A, RL); FZ = FN = FH = 0
#define RL_R(R) BASIC_OP_R(R, RL); RL_FLAGS
#define RL_MR(MR) BASIC_OP_MR(MR, RL); RL_FLAGS

#define RLC c = (u >> 7) & 1; u = (u << 1) | c; FC = c
#define RLC_FLAGS FZ_EQ0(u); FN = FH = 0
#define RLCA BASIC_OP_R(A, RLC); FZ = FN = FH = 0
#define RLC_R(R) BASIC_OP_R(R, RLC); RLC_FLAGS
#define RLC_MR(MR) BASIC_OP_MR(MR, RLC); RLC_FLAGS

#define RR c = u & 1; u = (FC << 7) | (u >> 1); FC = c
#define RR_FLAGS FZ_EQ0(u); FN = FH = 0
#define RRA BASIC_OP_R(A, RR); FZ = FN = FH = 0
#define RR_R(R) BASIC_OP_R(R, RR); RR_FLAGS
#define RR_MR(MR) BASIC_OP_MR(MR, RR); RR_FLAGS

#define RRC c = u & 1; u = (c << 7) | (u >> 1); FC = c
#define RRC_FLAGS FZ_EQ0(u); FN = FH = 0
#define RRCA BASIC_OP_R(A, RRC); FZ = FN = FH = 0
#define RRC_R(R) BASIC_OP_R(R, RRC); RRC_FLAGS
#define RRC_MR(MR) BASIC_OP_MR(MR, RRC); RRC_FLAGS

#define SCF FC = 1; FN = FH = 0

#define SET(BIT) u |= (1 << BIT)
#define SET_R(BIT, R) BASIC_OP_R(R, SET(BIT))
#define SET_MR(BIT, MR) BASIC_OP_MR(MR, SET(BIT))

#define SLA FC = (u >> 7) & 1; u <<= 1
#define SLA_FLAGS FZ_EQ0(u); FN = FH = 0
#define SLA_R(R) BASIC_OP_R(R, SLA); SLA_FLAGS
#define SLA_MR(MR) BASIC_OP_MR(MR, SLA); SLA_FLAGS

#define SRA FC = u & 1; u = (int8_t)u >> 1
#define SRA_FLAGS FZ_EQ0(u); FN = FH = 0
#define SRA_R(R) BASIC_OP_R(R, SRA); SRA_FLAGS
#define SRA_MR(MR) BASIC_OP_MR(MR, SRA); SRA_FLAGS

#define SRL FC = u & 1; u >>= 1
#define SRL_FLAGS FZ_EQ0(u); FN = FH = 0
#define SRL_R(R) BASIC_OP_R(R, SRL); SRL_FLAGS
#define SRL_MR(MR) BASIC_OP_MR(MR, SRL); SRL_FLAGS

#define SUB_FLAGS(X, Y) FZ_EQ0((X) - (Y)); FN = 1; FCH_SUB(X, Y)
#define SUB_R(R) SUB_FLAGS(RA, REG(R)); RA -= REG(R)
#define SUB_MR(MR) u = READMR(MR); SUB_FLAGS(RA, u); RA -= u
#define SUB_N u = READ_N; SUB_FLAGS(RA, u); RA -= u

#define SBC_FLAGS(X, Y, C) FZ_EQ0((X) - (Y) - (C)); FN = 1; FCH_SBC(X, Y, C)
#define SBC_R(R) u = REG(R); c = FC; SBC_FLAGS(RA, u, c); RA -= u + c
#define SBC_MR(MR) u = READMR(MR); c = FC; SBC_FLAGS(RA, u, c); RA -= u + c
#define SBC_N u = READ_N; c = FC; SBC_FLAGS(RA, u, c); RA -= u + c

#define SWAP u = (u << 4) | (u >> 4)
#define SWAP_FLAGS FZ_EQ0(u); FN = FH = FC = 0
#define SWAP_R(R) BASIC_OP_R(R, SWAP); SWAP_FLAGS
#define SWAP_MR(MR) BASIC_OP_MR(MR, SWAP); SWAP_FLAGS

#define XOR_FLAGS FZ_EQ0(RA); FN = FH = FC = 0
#define XOR_R(R) RA ^= REG(R); XOR_FLAGS
#define XOR_MR(MR) RA ^= READMR(MR); XOR_FLAGS
#define XOR_N RA ^= READ_N; XOR_FLAGS

#define TRACEI s_trace = TRUE; s_trace_counter = 2; print_emulator_info(e)

/* Returns the number of cycles executed */
uint8_t execute_instruction(struct Emulator* e) {
  uint8_t cycles = 0;
  int8_t s;
  uint8_t u;
  uint8_t c;
  uint8_t opcode = read_u8(e, e->reg.PC);
  Address new_pc = e->reg.PC + s_opcode_bytes[opcode];
  if (opcode == 0xcb) {
    uint8_t opcode = read_u8(e, e->reg.PC + 1);
    cycles = s_cb_opcode_cycles[opcode];
    switch (opcode) {
      case 0x00: RLC_R(B); break;
      case 0x01: RLC_R(C); break;
      case 0x02: RLC_R(D); break;
      case 0x03: RLC_R(E); break;
      case 0x04: RLC_R(H); break;
      case 0x05: RLC_R(L); break;
      case 0x06: RLC_MR(HL); break;
      case 0x07: RLC_R(A); break;
      case 0x08: RRC_R(B); break;
      case 0x09: RRC_R(C); break;
      case 0x0a: RRC_R(D); break;
      case 0x0b: RRC_R(E); break;
      case 0x0c: RRC_R(H); break;
      case 0x0d: RRC_R(L); break;
      case 0x0e: RRC_MR(HL); break;
      case 0x0f: RRC_R(A); break;
      case 0x10: RL_R(B); break;
      case 0x11: RL_R(C); break;
      case 0x12: RL_R(D); break;
      case 0x13: RL_R(E); break;
      case 0x14: RL_R(H); break;
      case 0x15: RL_R(L); break;
      case 0x16: RL_MR(HL); break;
      case 0x17: RL_R(A); break;
      case 0x18: RR_R(B); break;
      case 0x19: RR_R(C); break;
      case 0x1a: RR_R(D); break;
      case 0x1b: RR_R(E); break;
      case 0x1c: RR_R(H); break;
      case 0x1d: RR_R(L); break;
      case 0x1e: RR_MR(HL); break;
      case 0x1f: RR_R(A); break;
      case 0x20: SLA_R(B); break;
      case 0x21: SLA_R(C); break;
      case 0x22: SLA_R(D); break;
      case 0x23: SLA_R(E); break;
      case 0x24: SLA_R(H); break;
      case 0x25: SLA_R(L); break;
      case 0x26: SLA_MR(HL); break;
      case 0x27: SLA_R(A); break;
      case 0x28: SRA_R(B); break;
      case 0x29: SRA_R(C); break;
      case 0x2a: SRA_R(D); break;
      case 0x2b: SRA_R(E); break;
      case 0x2c: SRA_R(H); break;
      case 0x2d: SRA_R(L); break;
      case 0x2e: SRA_MR(HL); break;
      case 0x2f: SRA_R(A); break;
      case 0x30: SWAP_R(B); break;
      case 0x31: SWAP_R(C); break;
      case 0x32: SWAP_R(D); break;
      case 0x33: SWAP_R(E); break;
      case 0x34: SWAP_R(H); break;
      case 0x35: SWAP_R(L); break;
      case 0x36: SWAP_MR(HL); break;
      case 0x37: SWAP_R(A); break;
      case 0x38: SRL_R(B); break;
      case 0x39: SRL_R(C); break;
      case 0x3a: SRL_R(D); break;
      case 0x3b: SRL_R(E); break;
      case 0x3c: SRL_R(H); break;
      case 0x3d: SRL_R(L); break;
      case 0x3e: SRL_MR(HL); break;
      case 0x3f: SRL_R(A); break;
      case 0x40: BIT_R(0, B); break;
      case 0x41: BIT_R(0, C); break;
      case 0x42: BIT_R(0, D); break;
      case 0x43: BIT_R(0, E); break;
      case 0x44: BIT_R(0, H); break;
      case 0x45: BIT_R(0, L); break;
      case 0x46: BIT_MR(0, HL); break;
      case 0x47: BIT_R(0, A); break;
      case 0x48: BIT_R(1, B); break;
      case 0x49: BIT_R(1, C); break;
      case 0x4a: BIT_R(1, D); break;
      case 0x4b: BIT_R(1, E); break;
      case 0x4c: BIT_R(1, H); break;
      case 0x4d: BIT_R(1, L); break;
      case 0x4e: BIT_MR(1, HL); break;
      case 0x4f: BIT_R(1, A); break;
      case 0x50: BIT_R(2, B); break;
      case 0x51: BIT_R(2, C); break;
      case 0x52: BIT_R(2, D); break;
      case 0x53: BIT_R(2, E); break;
      case 0x54: BIT_R(2, H); break;
      case 0x55: BIT_R(2, L); break;
      case 0x56: BIT_MR(2, HL); break;
      case 0x57: BIT_R(2, A); break;
      case 0x58: BIT_R(3, B); break;
      case 0x59: BIT_R(3, C); break;
      case 0x5a: BIT_R(3, D); break;
      case 0x5b: BIT_R(3, E); break;
      case 0x5c: BIT_R(3, H); break;
      case 0x5d: BIT_R(3, L); break;
      case 0x5e: BIT_MR(3, HL); break;
      case 0x5f: BIT_R(3, A); break;
      case 0x60: BIT_R(4, B); break;
      case 0x61: BIT_R(4, C); break;
      case 0x62: BIT_R(4, D); break;
      case 0x63: BIT_R(4, E); break;
      case 0x64: BIT_R(4, H); break;
      case 0x65: BIT_R(4, L); break;
      case 0x66: BIT_MR(4, HL); break;
      case 0x67: BIT_R(4, A); break;
      case 0x68: BIT_R(5, B); break;
      case 0x69: BIT_R(5, C); break;
      case 0x6a: BIT_R(5, D); break;
      case 0x6b: BIT_R(5, E); break;
      case 0x6c: BIT_R(5, H); break;
      case 0x6d: BIT_R(5, L); break;
      case 0x6e: BIT_MR(5, HL); break;
      case 0x6f: BIT_R(5, A); break;
      case 0x70: BIT_R(6, B); break;
      case 0x71: BIT_R(6, C); break;
      case 0x72: BIT_R(6, D); break;
      case 0x73: BIT_R(6, E); break;
      case 0x74: BIT_R(6, H); break;
      case 0x75: BIT_R(6, L); break;
      case 0x76: BIT_MR(6, HL); break;
      case 0x77: BIT_R(6, A); break;
      case 0x78: BIT_R(7, B); break;
      case 0x79: BIT_R(7, C); break;
      case 0x7a: BIT_R(7, D); break;
      case 0x7b: BIT_R(7, E); break;
      case 0x7c: BIT_R(7, H); break;
      case 0x7d: BIT_R(7, L); break;
      case 0x7e: BIT_MR(7, HL); break;
      case 0x7f: BIT_R(7, A); break;
      case 0x80: RES_R(0, B); break;
      case 0x81: RES_R(0, C); break;
      case 0x82: RES_R(0, D); break;
      case 0x83: RES_R(0, E); break;
      case 0x84: RES_R(0, H); break;
      case 0x85: RES_R(0, L); break;
      case 0x86: RES_MR(0, HL); break;
      case 0x87: RES_R(0, A); break;
      case 0x88: RES_R(1, B); break;
      case 0x89: RES_R(1, C); break;
      case 0x8a: RES_R(1, D); break;
      case 0x8b: RES_R(1, E); break;
      case 0x8c: RES_R(1, H); break;
      case 0x8d: RES_R(1, L); break;
      case 0x8e: RES_MR(1, HL); break;
      case 0x8f: RES_R(1, A); break;
      case 0x90: RES_R(2, B); break;
      case 0x91: RES_R(2, C); break;
      case 0x92: RES_R(2, D); break;
      case 0x93: RES_R(2, E); break;
      case 0x94: RES_R(2, H); break;
      case 0x95: RES_R(2, L); break;
      case 0x96: RES_MR(2, HL); break;
      case 0x97: RES_R(2, A); break;
      case 0x98: RES_R(3, B); break;
      case 0x99: RES_R(3, C); break;
      case 0x9a: RES_R(3, D); break;
      case 0x9b: RES_R(3, E); break;
      case 0x9c: RES_R(3, H); break;
      case 0x9d: RES_R(3, L); break;
      case 0x9e: RES_MR(3, HL); break;
      case 0x9f: RES_R(3, A); break;
      case 0xa0: RES_R(4, B); break;
      case 0xa1: RES_R(4, C); break;
      case 0xa2: RES_R(4, D); break;
      case 0xa3: RES_R(4, E); break;
      case 0xa4: RES_R(4, H); break;
      case 0xa5: RES_R(4, L); break;
      case 0xa6: RES_MR(4, HL); break;
      case 0xa7: RES_R(4, A); break;
      case 0xa8: RES_R(5, B); break;
      case 0xa9: RES_R(5, C); break;
      case 0xaa: RES_R(5, D); break;
      case 0xab: RES_R(5, E); break;
      case 0xac: RES_R(5, H); break;
      case 0xad: RES_R(5, L); break;
      case 0xae: RES_MR(5, HL); break;
      case 0xaf: RES_R(5, A); break;
      case 0xb0: RES_R(6, B); break;
      case 0xb1: RES_R(6, C); break;
      case 0xb2: RES_R(6, D); break;
      case 0xb3: RES_R(6, E); break;
      case 0xb4: RES_R(6, H); break;
      case 0xb5: RES_R(6, L); break;
      case 0xb6: RES_MR(6, HL); break;
      case 0xb7: RES_R(6, A); break;
      case 0xb8: RES_R(7, B); break;
      case 0xb9: RES_R(7, C); break;
      case 0xba: RES_R(7, D); break;
      case 0xbb: RES_R(7, E); break;
      case 0xbc: RES_R(7, H); break;
      case 0xbd: RES_R(7, L); break;
      case 0xbe: RES_MR(7, HL); break;
      case 0xbf: RES_R(7, A); break;
      case 0xc0: SET_R(0, B); break;
      case 0xc1: SET_R(0, C); break;
      case 0xc2: SET_R(0, D); break;
      case 0xc3: SET_R(0, E); break;
      case 0xc4: SET_R(0, H); break;
      case 0xc5: SET_R(0, L); break;
      case 0xc6: SET_MR(0, HL); break;
      case 0xc7: SET_R(0, A); break;
      case 0xc8: SET_R(1, B); break;
      case 0xc9: SET_R(1, C); break;
      case 0xca: SET_R(1, D); break;
      case 0xcb: SET_R(1, E); break;
      case 0xcc: SET_R(1, H); break;
      case 0xcd: SET_R(1, L); break;
      case 0xce: SET_MR(1, HL); break;
      case 0xcf: SET_R(1, A); break;
      case 0xd0: SET_R(2, B); break;
      case 0xd1: SET_R(2, C); break;
      case 0xd2: SET_R(2, D); break;
      case 0xd3: SET_R(2, E); break;
      case 0xd4: SET_R(2, H); break;
      case 0xd5: SET_R(2, L); break;
      case 0xd6: SET_MR(2, HL); break;
      case 0xd7: SET_R(2, A); break;
      case 0xd8: SET_R(3, B); break;
      case 0xd9: SET_R(3, C); break;
      case 0xda: SET_R(3, D); break;
      case 0xdb: SET_R(3, E); break;
      case 0xdc: SET_R(3, H); break;
      case 0xdd: SET_R(3, L); break;
      case 0xde: SET_MR(3, HL); break;
      case 0xdf: SET_R(3, A); break;
      case 0xe0: SET_R(4, B); break;
      case 0xe1: SET_R(4, C); break;
      case 0xe2: SET_R(4, D); break;
      case 0xe3: SET_R(4, E); break;
      case 0xe4: SET_R(4, H); break;
      case 0xe5: SET_R(4, L); break;
      case 0xe6: SET_MR(4, HL); break;
      case 0xe7: SET_R(4, A); break;
      case 0xe8: SET_R(5, B); break;
      case 0xe9: SET_R(5, C); break;
      case 0xea: SET_R(5, D); break;
      case 0xeb: SET_R(5, E); break;
      case 0xec: SET_R(5, H); break;
      case 0xed: SET_R(5, L); break;
      case 0xee: SET_MR(5, HL); break;
      case 0xef: SET_R(5, A); break;
      case 0xf0: SET_R(6, B); break;
      case 0xf1: SET_R(6, C); break;
      case 0xf2: SET_R(6, D); break;
      case 0xf3: SET_R(6, E); break;
      case 0xf4: SET_R(6, H); break;
      case 0xf5: SET_R(6, L); break;
      case 0xf6: SET_MR(6, HL); break;
      case 0xf7: SET_R(6, A); break;
      case 0xf8: SET_R(7, B); break;
      case 0xf9: SET_R(7, C); break;
      case 0xfa: SET_R(7, D); break;
      case 0xfb: SET_R(7, E); break;
      case 0xfc: SET_R(7, H); break;
      case 0xfd: SET_R(7, L); break;
      case 0xfe: SET_MR(7, HL); break;
      case 0xff: SET_R(7, A); break;
    }
  } else {
    cycles = s_opcode_cycles[opcode];
    switch (opcode) {
      case 0x00: break;
      case 0x01: LD_RR_NN(BC); break;
      case 0x02: LD_MR_R(BC, A); break;
      case 0x03: INC_RR(BC); break;
      case 0x04: INC_R(B); break;
      case 0x05: DEC_R(B); break;
      case 0x06: LD_R_N(B); break;
      case 0x07: RLCA; break;
      case 0x08: LD_MNN_SP; break;
      case 0x09: ADD_HL_RR(BC); break;
      case 0x0a: LD_R_MR(A, BC); break;
      case 0x0b: DEC_RR(BC); break;
      case 0x0c: INC_R(C); break;
      case 0x0d: DEC_R(C); break;
      case 0x0e: LD_R_N(C); break;
      case 0x0f: RRCA; break;
      case 0x10: NI; break;
      case 0x11: LD_RR_NN(DE); break;
      case 0x12: LD_MR_R(DE, A); break;
      case 0x13: INC_RR(DE); break;
      case 0x14: INC_R(D); break;
      case 0x15: DEC_R(D); break;
      case 0x16: LD_R_N(D); break;
      case 0x17: RLA; break;
      case 0x18: JR_N; break;
      case 0x19: ADD_HL_RR(DE); break;
      case 0x1a: LD_R_MR(A, DE); break;
      case 0x1b: DEC_RR(DE); break;
      case 0x1c: INC_R(E); break;
      case 0x1d: DEC_R(E); break;
      case 0x1e: LD_R_N(E); break;
      case 0x1f: RRA; break;
      case 0x20: JR_F_N(COND_NZ); break;
      case 0x21: LD_RR_NN(HL); break;
      case 0x22: LD_MR_R(HL, A); REG(HL)++; break;
      case 0x23: INC_RR(HL); break;
      case 0x24: INC_R(H); break;
      case 0x25: DEC_R(H); break;
      case 0x26: LD_R_N(H); break;
      case 0x27: DAA; break;
      case 0x28: JR_F_N(COND_Z); break;
      case 0x29: ADD_HL_RR(HL); break;
      case 0x2a: LD_R_MR(A, HL); REG(HL)++; break;
      case 0x2b: DEC_RR(HL); break;
      case 0x2c: INC_R(L); break;
      case 0x2d: DEC_R(L); break;
      case 0x2e: LD_R_N(L); break;
      case 0x2f: CPL; break;
      case 0x30: JR_F_N(COND_NC); break;
      case 0x31: LD_RR_NN(SP); break;
      case 0x32: LD_MR_R(HL, A); REG(HL)--; break;
      case 0x33: INC_RR(SP); break;
      case 0x34: INC_MR(HL); break;
      case 0x35: DEC_MR(HL); break;
      case 0x36: LD_MR_N(HL); break;
      case 0x37: SCF; break;
      case 0x38: JR_F_N(COND_C); break;
      case 0x39: ADD_HL_RR(SP); break;
      case 0x3a: LD_R_MR(A, HL); REG(HL)--; break;
      case 0x3b: DEC_RR(SP); break;
      case 0x3c: INC_R(A); break;
      case 0x3d: DEC_R(A); break;
      case 0x3e: LD_R_N(A); break;
      case 0x3f: CCF; break;
      case 0x40: LD_R_R(B, B); break;
      case 0x41: LD_R_R(B, C); break;
      case 0x42: LD_R_R(B, D); break;
      case 0x43: LD_R_R(B, E); break;
      case 0x44: LD_R_R(B, H); break;
      case 0x45: LD_R_R(B, L); break;
      case 0x46: LD_R_MR(B, HL); break;
      case 0x47: LD_R_R(B, A); break;
      case 0x48: LD_R_R(C, B); break;
      case 0x49: LD_R_R(C, C); break;
      case 0x4a: LD_R_R(C, D); break;
      case 0x4b: LD_R_R(C, E); break;
      case 0x4c: LD_R_R(C, H); break;
      case 0x4d: LD_R_R(C, L); break;
      case 0x4e: LD_R_MR(C, HL); break;
      case 0x4f: LD_R_R(C, A); break;
      case 0x50: LD_R_R(D, B); break;
      case 0x51: LD_R_R(D, C); break;
      case 0x52: LD_R_R(D, D); break;
      case 0x53: LD_R_R(D, E); break;
      case 0x54: LD_R_R(D, H); break;
      case 0x55: LD_R_R(D, L); break;
      case 0x56: LD_R_MR(D, HL); break;
      case 0x57: LD_R_R(D, A); break;
      case 0x58: LD_R_R(E, B); break;
      case 0x59: LD_R_R(E, C); break;
      case 0x5a: LD_R_R(E, D); break;
      case 0x5b: LD_R_R(E, E); break;
      case 0x5c: LD_R_R(E, H); break;
      case 0x5d: LD_R_R(E, L); break;
      case 0x5e: LD_R_MR(E, HL); break;
      case 0x5f: LD_R_R(E, A); break;
      case 0x60: LD_R_R(H, B); break;
      case 0x61: LD_R_R(H, C); break;
      case 0x62: LD_R_R(H, D); break;
      case 0x63: LD_R_R(H, E); break;
      case 0x64: LD_R_R(H, H); break;
      case 0x65: LD_R_R(H, L); break;
      case 0x66: LD_R_MR(H, HL); break;
      case 0x67: LD_R_R(H, A); break;
      case 0x68: LD_R_R(L, B); break;
      case 0x69: LD_R_R(L, C); break;
      case 0x6a: LD_R_R(L, D); break;
      case 0x6b: LD_R_R(L, E); break;
      case 0x6c: LD_R_R(L, H); break;
      case 0x6d: LD_R_R(L, L); break;
      case 0x6e: LD_R_MR(L, HL); break;
      case 0x6f: LD_R_R(L, A); break;
      case 0x70: LD_MR_R(HL, B); break;
      case 0x71: LD_MR_R(HL, C); break;
      case 0x72: LD_MR_R(HL, D); break;
      case 0x73: LD_MR_R(HL, E); break;
      case 0x74: LD_MR_R(HL, H); break;
      case 0x75: LD_MR_R(HL, L); break;
      case 0x76: NI; break;
      case 0x77: LD_MR_R(HL, A); break;
      case 0x78: LD_R_R(A, B); break;
      case 0x79: LD_R_R(A, C); break;
      case 0x7a: LD_R_R(A, D); break;
      case 0x7b: LD_R_R(A, E); break;
      case 0x7c: LD_R_R(A, H); break;
      case 0x7d: LD_R_R(A, L); break;
      case 0x7e: LD_R_MR(A, HL); break;
      case 0x7f: LD_R_R(A, A); break;
      case 0x80: ADD_R(B); break;
      case 0x81: ADD_R(C); break;
      case 0x82: ADD_R(D); break;
      case 0x83: ADD_R(E); break;
      case 0x84: ADD_R(H); break;
      case 0x85: ADD_R(L); break;
      case 0x86: ADD_MR(HL); break;
      case 0x87: ADD_R(A); break;
      case 0x88: ADC_R(B); break;
      case 0x89: ADC_R(C); break;
      case 0x8a: ADC_R(D); break;
      case 0x8b: ADC_R(E); break;
      case 0x8c: ADC_R(H); break;
      case 0x8d: ADC_R(L); break;
      case 0x8e: ADC_MR(HL); break;
      case 0x8f: ADC_R(A); break;
      case 0x90: SUB_R(B); break;
      case 0x91: SUB_R(C); break;
      case 0x92: SUB_R(D); break;
      case 0x93: SUB_R(E); break;
      case 0x94: SUB_R(H); break;
      case 0x95: SUB_R(L); break;
      case 0x96: SUB_MR(HL); break;
      case 0x97: SUB_R(A); break;
      case 0x98: SBC_R(B); break;
      case 0x99: SBC_R(C); break;
      case 0x9a: SBC_R(D); break;
      case 0x9b: SBC_R(E); break;
      case 0x9c: SBC_R(H); break;
      case 0x9d: SBC_R(L); break;
      case 0x9e: SBC_MR(HL); break;
      case 0x9f: SBC_R(A); break;
      case 0xa0: AND_R(B); break;
      case 0xa1: AND_R(C); break;
      case 0xa2: AND_R(D); break;
      case 0xa3: AND_R(E); break;
      case 0xa4: AND_R(H); break;
      case 0xa5: AND_R(L); break;
      case 0xa6: AND_MR(HL); break;
      case 0xa7: AND_R(A); break;
      case 0xa8: XOR_R(B); break;
      case 0xa9: XOR_R(C); break;
      case 0xaa: XOR_R(D); break;
      case 0xab: XOR_R(E); break;
      case 0xac: XOR_R(H); break;
      case 0xad: XOR_R(L); break;
      case 0xae: XOR_MR(HL); break;
      case 0xaf: XOR_R(A); break;
      case 0xb0: OR_R(B); break;
      case 0xb1: OR_R(C); break;
      case 0xb2: OR_R(D); break;
      case 0xb3: OR_R(E); break;
      case 0xb4: OR_R(H); break;
      case 0xb5: OR_R(L); break;
      case 0xb6: OR_MR(HL); break;
      case 0xb7: OR_R(A); break;
      case 0xb8: CP_R(B); break;
      case 0xb9: CP_R(C); break;
      case 0xba: CP_R(D); break;
      case 0xbb: CP_R(E); break;
      case 0xbc: CP_R(H); break;
      case 0xbd: CP_R(L); break;
      case 0xbe: CP_MR(HL); break;
      case 0xbf: CP_R(A); break;
      case 0xc0: RET_F(COND_NZ); break;
      case 0xc1: POP_RR(BC); break;
      case 0xc2: JP_F_NN(COND_NZ); break;
      case 0xc3: JP_NN; break;
      case 0xc4: CALL_F_NN(COND_NZ); break;
      case 0xc5: PUSH_RR(BC); break;
      case 0xc6: ADD_N; break;
      case 0xc7: CALL(0x00); break;
      case 0xc8: RET_F(COND_Z); break;
      case 0xc9: RET; break;
      case 0xca: JP_F_NN(COND_Z); break;
      case 0xcb: INVALID; break;
      case 0xcc: CALL_F_NN(COND_Z); break;
      case 0xcd: CALL_NN; break;
      case 0xce: ADC_N; break;
      case 0xcf: CALL(0x08); break;
      case 0xd0: RET_F(COND_NC); break;
      case 0xd1: POP_RR(DE); break;
      case 0xd2: JP_F_NN(COND_NC); break;
      case 0xd3: INVALID; break;
      case 0xd4: CALL_F_NN(COND_NC); break;
      case 0xd5: PUSH_RR(DE); break;
      case 0xd6: SUB_N; break;
      case 0xd7: CALL(0x10); break;
      case 0xd8: RET_F(COND_C); break;
      case 0xd9: RETI; break;
      case 0xda: JP_F_NN(COND_C); break;
      case 0xdb: INVALID; break;
      case 0xdc: CALL_F_NN(COND_C); break;
      case 0xdd: INVALID; break;
      case 0xde: SBC_N; break;
      case 0xdf: CALL(0x18); break;
      case 0xe0: LD_MFF00_N_R(A); break;
      case 0xe1: POP_RR(HL); break;
      case 0xe2: LD_MFF00_R_R(C, A); break;
      case 0xe3: INVALID; break;
      case 0xe4: INVALID; break;
      case 0xe5: PUSH_RR(HL); break;
      case 0xe6: AND_N; break;
      case 0xe7: CALL(0x20); break;
      case 0xe8: ADD_SP_N; break;
      case 0xe9: JP_RR(HL); break;
      case 0xea: LD_MN_R(A); break;
      case 0xeb: INVALID; break;
      case 0xec: INVALID; break;
      case 0xed: INVALID; break;
      case 0xee: XOR_N; break;
      case 0xef: CALL(0x28); break;
      case 0xf0: LD_R_MFF00_N(A); break;
      case 0xf1: POP_AF; break;
      case 0xf2: LD_R_MFF00_R(A, C); break;
      case 0xf3: e->interrupts.IME = FALSE; break;
      case 0xf4: INVALID; break;
      case 0xf5: PUSH_AF; break;
      case 0xf6: OR_N; break;
      case 0xf7: CALL(0x30); break;
      case 0xf8: LD_HL_SP_N; break;
      case 0xf9: LD_RR_RR(SP, HL); break;
      case 0xfa: LD_R_MN(A); break;
      case 0xfb: e->interrupts.IME = TRUE; break;
      case 0xfc: INVALID; break;
      case 0xfd: INVALID; break;
      case 0xfe: CP_N; break;
      case 0xff: CALL(0x38); break;
      default: INVALID; break;
    }
  }
  e->reg.PC = new_pc;
  return cycles;
}

enum Result write_frame_ppm(struct Emulator* e) {
  char filename[100];
  snprintf(filename, sizeof(filename), "frame%05u.ppm", e->lcd.frame);
  FILE* f = fopen(filename, "wb");
  CHECK_MSG(f, "unable to open file \"%s\".\n", filename);
  CHECK_MSG(fputs("P3\n160 144\n255\n", f) >= 0, "fputs failed.\n");
  uint8_t x, y;
  RGBA* data = &e->frame_buffer[0];
  for (y = 0; y < SCREEN_HEIGHT; ++y) {
    for (x = 0; x < SCREEN_WIDTH; ++x) {
      RGBA pixel = *data++;
      uint8_t b = (pixel >> 16) & 0xff;
      uint8_t g = (pixel >> 8) & 0xff;
      uint8_t r = (pixel >> 0) & 0xff;
      CHECK_MSG(fprintf(f, "%3u %3u %3u ", r, g, b) >= 0, "fprintf failed.\n");
    }
    CHECK_MSG(fputs("\n", f) >= 0, "fputs failed.\n");
  }
  fclose(f);
  return OK;
error:
  if (f) {
    fclose(f);
  }
  return ERROR;
}

enum Result write_tile_data_ppm(struct Emulator* e) {
  char filename[100];
  snprintf(filename, sizeof(filename), "tiledata%05u.ppm", e->lcd.frame);
  FILE* f = fopen(filename, "wb");
  CHECK_MSG(f, "unable to open file \"%s\".\n", filename);
  CHECK_MSG(fputs("P3\n192 128\n255\n", f) >= 0, "fputs failed.\n");
  uint32_t x, y;
  for (y = 0; y < 16 * 8; ++y) {
    for (x = 0; x < 24 * 8; ++x) {
      uint8_t tile_x = x >> 3;
      uint8_t tile_y = y >> 3;
      Tile* tile = &e->vram.tile[tile_y * 8 + tile_x];
      uint8_t palette_index = (*tile)[(y & 7) * 8 + (x & 7)];
      enum Color color = e->lcd.bgp.color[palette_index];
      RGBA pixel = s_color_to_rgba[color];
      uint8_t b = (pixel >> 16) & 0xff;
      uint8_t g = (pixel >> 8) & 0xff;
      uint8_t r = (pixel >> 0) & 0xff;
      CHECK_MSG(fprintf(f, "%3u %3u %3u ", r, g, b) >= 0, "fprintf failed.\n");
    }
    CHECK_MSG(fputs("\n", f) >= 0, "fputs failed.\n");
  }
  fclose(f);
  return OK;
error:
  if (f) {
    fclose(f);
  }
  return ERROR;
}

void new_frame(struct Emulator* e) {
  e->lcd.win_y = 0;
  e->lcd.frame_WY = e->lcd.WY;
  e->lcd.frame++;
  e->lcd.new_frame_edge = TRUE;
}

void update_dma_cycles(struct Emulator* e, uint8_t cycles) {
  if (!e->dma.active) {
    return;
  }

  e->dma.cycles += cycles;
  if (e->dma.cycles >= DMA_CYCLES) {
    e->dma.active = FALSE;
    Address i;
    for (i = 0; i < OAM_TRANSFER_SIZE; ++i) {
      write_u8(e, OAM_START_ADDR + i, read_u8(e, e->dma.addr + i));
    }
  }
}

void update_lcd_cycles(struct Emulator* e, uint8_t cycles) {
  enum Bool line_edge = FALSE;
  e->lcd.new_frame_edge = FALSE;
  e->lcd.cycles += cycles;
  switch (e->lcd.stat.mode) {
    case LCD_MODE_USING_OAM:
      if (e->lcd.cycles >= USING_OAM_CYCLES) {
        render_line(e);
        e->lcd.cycles -= USING_OAM_CYCLES;
        e->lcd.stat.mode = LCD_MODE_USING_OAM_VRAM;
      }
      break;
    case LCD_MODE_USING_OAM_VRAM:
      if (e->lcd.cycles >= USING_OAM_VRAM_CYCLES) {
        e->lcd.cycles -= USING_OAM_VRAM_CYCLES;
        e->lcd.stat.mode = LCD_MODE_HBLANK;
        if (e->lcd.stat.hblank_intr) {
          e->interrupts.IF |= INTERRUPT_LCD_STAT_MASK;
        }
      }
      break;
    case LCD_MODE_HBLANK:
      if (e->lcd.cycles >= HBLANK_CYCLES) {
        line_edge = TRUE;
        e->lcd.cycles -= HBLANK_CYCLES;
        e->lcd.LY++;
        if (e->lcd.LY == 144) {
          e->lcd.stat.mode = LCD_MODE_VBLANK;
          e->interrupts.IF |= INTERRUPT_VBLANK_MASK;
          if (e->lcd.stat.vblank_intr) {
            e->interrupts.IF |= INTERRUPT_LCD_STAT_MASK;
          }
        } else {
          e->lcd.stat.mode = LCD_MODE_USING_OAM;
          if (e->lcd.stat.using_oam_intr) {
            e->interrupts.IF |= INTERRUPT_LCD_STAT_MASK;
          }
        }
      }
      break;
    case LCD_MODE_VBLANK:
      if (e->lcd.cycles >= LINE_CYCLES) {
        line_edge = TRUE;
        e->lcd.cycles -= LINE_CYCLES;
        e->lcd.LY++;
        if (e->lcd.LY == 154) {
          new_frame(e);
          e->lcd.LY = 0;
          e->lcd.stat.mode = LCD_MODE_USING_OAM;
          if (e->lcd.stat.using_oam_intr) {
            e->interrupts.IF |= INTERRUPT_LCD_STAT_MASK;
          }
        }
      }
      break;
  }
  if (line_edge && e->lcd.stat.y_compare_intr && e->lcd.LY == e->lcd.LYC) {
    e->interrupts.IF |= INTERRUPT_LCD_STAT_MASK;
  }
}

void update_timer_cycles(struct Emulator* e, uint8_t cycles) {
  e->timer.div_cycles += cycles;
  if (e->timer.div_cycles >= DIV_CYCLES) {
    e->timer.div_cycles -= DIV_CYCLES;
    e->timer.DIV++;
  }

  if (e->timer.on) {
    e->timer.tima_cycles += cycles;
    uint32_t tima_max_cycles = 0;
    switch (e->timer.input_clock_select) {
      case TIMER_CLOCK_4096_HZ:
        tima_max_cycles = TIMA_4096_CYCLES;
        break;
      case TIMER_CLOCK_262144_HZ:
        tima_max_cycles = TIMA_262144_CYCLES;
        break;
      case TIMER_CLOCK_65536_HZ:
        tima_max_cycles = TIMA_65536_CYCLES;
        break;
      case TIMER_CLOCK_16384_HZ:
        tima_max_cycles = TIMA_16384_CYCLES;
        break;
    }
    if (e->timer.tima_cycles >= tima_max_cycles) {
      e->timer.tima_cycles -= tima_max_cycles;
      e->timer.TIMA++;
      if (e->timer.TIMA == 0) {
        e->timer.TIMA = e->timer.TMA;
        e->interrupts.IF |= INTERRUPT_TIMER_MASK;
      }
    }
  }
}

void handle_interrupts(struct Emulator* e) {
  if (e->interrupts.IME == 0) {
    return;
  }

  uint8_t interrupts = e->interrupts.IF & e->interrupts.IE;
  if (interrupts == 0) {
    return;
  }

  Address vector;
  if (interrupts & INTERRUPT_VBLANK_MASK) {
    DEBUG(">> VBLANK interrupt [frame = %u]\n", e->lcd.frame);
    vector = 0x40;
    e->interrupts.IF &= ~INTERRUPT_VBLANK_MASK;
  } else if (interrupts & INTERRUPT_LCD_STAT_MASK) {
    DEBUG(">> LCD_STAT interrupt\n");
    vector = 0x48;
    e->interrupts.IF &= ~INTERRUPT_LCD_STAT_MASK;
  } else if (interrupts & INTERRUPT_TIMER_MASK) {
    DEBUG(">> TIMER interrupt\n");
    vector = 0x50;
    e->interrupts.IF &= ~INTERRUPT_TIMER_MASK;
  } else {
    return;
  }

  Address new_pc = REG(PC);
  CALL(vector);
  REG(PC) = new_pc;
  e->interrupts.IME = 0;
}

int main(int argc, char** argv) {
  --argc; ++argv;
  int result = 1;

  struct RomData rom_data;
  struct Emulator emulator;
  ZERO_MEMORY(emulator);
  struct Emulator* e = &emulator;
  CHECK_MSG(argc == 1, "no rom file given.\n");
  CHECK(SUCCESS(read_rom_data_from_file(argv[0], &rom_data)));
  CHECK(SUCCESS(init_emulator(e, &rom_data)));

  CHECK_MSG(SDL_Init(SDL_INIT_EVERYTHING) == 0, "SDL_init failed.\n");
  SDL_Surface* surface = SDL_SetVideoMode(RENDER_WIDTH, RENDER_HEIGHT, 32, 0);
  CHECK_MSG(surface != NULL, "SDL_SetVideoMode failed.\n");

  enum Bool running = TRUE;
  while (running) {
    print_emulator_info(e);
    uint8_t cycles = execute_instruction(e);
    update_dma_cycles(e, cycles);
    update_lcd_cycles(e, cycles);
    update_timer_cycles(e, cycles);
    handle_interrupts(e);

    if (e->lcd.new_frame_edge) {
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        switch (event.type) {
          case SDL_QUIT:
            running = FALSE;
            break;
          default:
            break;
        }
      }

      if (SDL_LockSurface(surface) == 0) {
        uint32_t* pixels = surface->pixels;
        int sx, sy;
        for (sy = 0; sy < SCREEN_HEIGHT; sy++) {
          for (sx = 0; sx < SCREEN_WIDTH; sx++) {
            int i, j;
            RGBA pixel = e->frame_buffer[sy * SCREEN_WIDTH + sx];
            uint8_t r = (pixel >> 16) & 0xff;
            uint8_t g = (pixel >> 8) & 0xff;
            uint8_t b = (pixel >> 0) & 0xff;
            uint32_t mapped = SDL_MapRGB(surface->format, r, g, b);
            for (j = 0; j < RENDER_SCALE; j++) {
              for (i = 0; i < RENDER_SCALE; i++) {
                int rx = sx * RENDER_SCALE + i;
                int ry = sy * RENDER_SCALE + j;
                pixels[ry * RENDER_WIDTH + rx] = mapped;
              }
            }
          }
        }
        SDL_UnlockSurface(surface);
      }
      SDL_Flip(surface);
    }

  }

  result = 0;
error:
  if (surface) {
    SDL_FreeSurface(surface);
  }
  SDL_Quit();
  return result;
}
