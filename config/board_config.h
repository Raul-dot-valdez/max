/*
 * Board configuration: AI Terra 7" mower HMI, rev A.
 * Logical resource numbers used by app code; the PAL port maps them to
 * peripherals. Everything a hardware respin may change lives here.
 */
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/* ---- Bill of materials (reference parts) ------------------------------ */
/* MCU   : STM32H743 (2 MB flash, 1 MB SRAM), -40..125 °C grade, run at 400 MHz (VOS1) */
/* SDRAM : ISSI IS42S32800J / IS45S32800J (automotive A2), 8M x 32 = 32 MB on FMC SDRAM bank 1 */
/* NOR   : Winbond W25Q512JV, 64 MB, QUADSPI bank 1                                  */
/* RTC   : Micro Crystal RV-3028-C7 (AEC-Q200), I2C 0x52, supercap/coin backup         */
/* BLE   : Infineon (Cypress) PSoC 4 BLE CY8C4248LQI-BL583, Bluetooth 4.2, UART link  */
/* GNSS  : u-blox M10 class receiver, UART NMEA 115200, 5 Hz                          */
/* LCD   : 7" 800x480 RGB888 TFT, >=1000 cd/m2 sunlight readable, LTDC                 */
/* Touch : Goodix GT911 PCAP (glove/wet tuned), I2C 0x5D                                */
/* CAN   : TCAN1042-Q1 transceiver, J1939 250 kbit/s (J1939-11), 500k option (-14)    */

/* ---- CAN ---------------------------------------------------------------- */
#define BOARD_CAN_VEHICLE         0
#define BOARD_CAN_BITRATE         250000u
#define BOARD_J1939_PREFERRED_SA  0x28u  /* 40: Cab Display #1 */
#define BOARD_J1939_ARBITRARY     1
/* J1939 NAME: AAC=1, industry group 2 (agricultural & forestry; use 1 for
 * on-highway, 3 for construction), function 60 (cab display). Manufacturer
 * code must be the one SAE assigns to AI Terra — 0x7FF is a placeholder. */
#define BOARD_J1939_MFG_CODE      0x7FFu
#define BOARD_J1939_FUNCTION      60u
#define BOARD_J1939_INDUSTRY      2u

/* ---- UARTs -------------------------------------------------------------- */
#define BOARD_UART_GNSS           0
#define BOARD_UART_GNSS_BAUD      115200u
#define BOARD_UART_BLE            1
#define BOARD_UART_BLE_BAUD       921600u

/* ---- I2C ---------------------------------------------------------------- */
#define BOARD_I2C_SYS             0      /* RTC + touch */
#define BOARD_I2C_HZ              400000u

/* ---- Display -------------------------------------------------------------- */
#define BOARD_LCD_WIDTH           800u
#define BOARD_LCD_HEIGHT          480u
#define BOARD_LCD_BPP             2u     /* RGB565 framebuffer, panel wired RGB888 */
#define BOARD_FB_BYTES            (BOARD_LCD_WIDTH * BOARD_LCD_HEIGHT * BOARD_LCD_BPP)

/* ---- Memory map ------------------------------------------------------------ */
/* Internal flash, 2 x 1 MB banks, 128 KB sectors */
#define BOARD_FLASH_BOOT_ADDR     0x08000000u /* B1 S0: bootloader, 128 KB */
#define BOARD_FLASH_APP_ADDR      0x08020000u /* B1 S1 .. B2 S5: application, 1664 KB */
#define BOARD_FLASH_NV0_ADDR      0x081C0000u /* B2 S6: nvstore A */
#define BOARD_FLASH_NV1_ADDR      0x081E0000u /* B2 S7: nvstore B */
#define BOARD_FLASH_SECTOR        0x20000u

/* External NOR partitions (W25Q512JV, 64 MB, 4 KB erase) */
#define BOARD_NOR_SIZE            (64u * 1024u * 1024u)
#define BOARD_NOR_ASSETS_OFF      0x00000000u /* 16 MB GUI bitmaps (cached to SDRAM at boot) */
#define BOARD_NOR_ASSETS_SIZE     0x01000000u
#define BOARD_NOR_FWSTAGE_OFF     0x01000000u /* 4 MB firmware update staging (bootloader copies) */
#define BOARD_NOR_FWSTAGE_SIZE    0x00400000u
#define BOARD_NOR_LOG_OFF         0x01400000u /* 44 MB fleet data log ring */
#define BOARD_NOR_LOG_SIZE        0x02C00000u

/* External SDRAM (IS42S32800J, 32 MB at 0xC0000000) */
#define BOARD_SDRAM_BASE          0xC0000000u
#define BOARD_SDRAM_SIZE          (32u * 1024u * 1024u)
#define BOARD_FB0_ADDR            BOARD_SDRAM_BASE                    /* 750 KB */
#define BOARD_FB1_ADDR            (BOARD_SDRAM_BASE + 0x000C0000u)    /* 750 KB */
#define BOARD_GUI_CACHE_ADDR      (BOARD_SDRAM_BASE + 0x00200000u)    /* 16 MB bitmap cache */
#define BOARD_GUI_CACHE_SIZE      0x01000000u
/* 0xC1200000.. : .sdram_bss (log sector index, coverage grid, GUI heap) */

/* ---- Application parameters (defaults; overridable over BLE) ------------- */
#define APP_TANK_L_DEFAULT        45.0f   /* zero-turn commercial mower, diesel */
#define APP_LOW_FUEL_PCT          15.0f
#define APP_RESERVE_PCT           5.0f
#define APP_SERVICE_INTERVAL_H    250.0f
#define APP_DECK_WIDTH_M          1.52f   /* 60 in deck */

#endif /* BOARD_CONFIG_H */
