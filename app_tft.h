#ifndef APP_TFT_H
#define APP_TFT_H

#include <stdbool.h>
#include <stdint.h>

/* =========================================================================
 * Dacai (Dacai) basic serial protocol constants
 *   Frame: [0xEE] [LEN] [CMD] [DATA...] [0xFF 0xFC 0xFF 0xFF]
 *   LEN = number of bytes in (CMD + DATA)
 * ========================================================================= */
#define TFT_FRAME_HEAD0             0xEEU
#define TFT_FRAME_TAIL0             0xFFU
#define TFT_FRAME_TAIL1             0xFCU
#define TFT_FRAME_TAIL2             0xFFU
#define TFT_FRAME_TAIL3             0xFFU

/* Dacai (DWIN) basic protocol — widget class byte + operation subcommand.
 *   Frame: [0xEE] [CLASS] [SUBCMD] [PAGE_HI] [PAGE_LO] [ID_HI] [ID_LO] [DATA...] [0xFF 0xFC 0xFF 0xFF]
 *   Confirmed against the official debug tool screenshots:
 *     - Button write:  EE B1 10 00 00 00 01 00 FF FC FF FF
 *     - Button read :  EE B1 11 00 00 00 01    FF FC FF FF
 *   Text uses 0xB0 (text-display widget), waveform uses 0xB3 (curve widget). */
#define TFT_WIDGET_TEXT             0xB0U
#define TFT_WIDGET_BUTTON           0xB1U
#define TFT_WIDGET_CURVE            0xB3U
#define TFT_WIDGET_PAGE             0xB1U   /* also page via 0xB1 + subcommand */

#define TFT_SUBCMD_WRITE            0x10U   /* write/update data */
#define TFT_SUBCMD_READ             0x11U   /* read current value */

#define TFT_BUTTON_PRESSED          0x01U
#define TFT_BUTTON_RELEASED         0x00U

/* Inter-byte pacing: 115200 baud @ BUSCLK 32 MHz = ~86.8 us per byte.
 * Per UART.md, 3200 cycles is the documented safe value for BUSCLK-sourced
 * UART at 115200. Includes start + stop + small margin. */
#define TFT_TX_BYTE_DELAY_CYCLES    3200U

/* RX ring buffer for parsing incoming 0xA1 button-state responses */
#define TFT_RX_RING_SIZE            256U
#define TFT_TX_BUF_SIZE             128U
#define TFT_BUTTON_TABLE_SIZE       64U

/* One-period waveform sample cap. Keeps the TX frame under
 * TFT_TX_BUF_SIZE / 2 = 64 16-bit samples; increase TX_BUF_SIZE for more. */
#define TFT_WAVE_MAX_POINTS         120U
#define TFT_WAVE_CHANNEL_0          0U

/* =========================================================================
 * Page IDs
 * ========================================================================= */
#define TFT_PAGE_MAIN               0U
#define TFT_PAGE_WAVEFORM           1U
#define TFT_PAGE_HARMONIC           2U

/* =========================================================================
 * Screen 1 (Main page) control IDs
 * ========================================================================= */
#define TFT_ID_MAIN_TITLE           0U
#define TFT_ID_MAIN_THD             1U
#define TFT_ID_MAIN_VPP             2U
#define TFT_ID_MAIN_F0              3U
#define TFT_ID_MAIN_T               4U
#define TFT_ID_MAIN_BTN_MEASURE     16U
#define TFT_ID_MAIN_BTN_WAVEFORM    17U
#define TFT_ID_MAIN_BTN_HARMONIC    18U

/* =========================================================================
 * Screen 2 (Waveform page) control IDs
 * ========================================================================= */
#define TFT_ID_WAVE_GRAPH           32U
#define TFT_ID_WAVE_VMAX            33U
#define TFT_ID_WAVE_VMIN            34U
#define TFT_ID_WAVE_VPP             35U
#define TFT_ID_WAVE_BTN_BACK        36U

/* =========================================================================
 * Screen 3 (Harmonic page) control IDs
 * ========================================================================= */
#define TFT_ID_HARM_TITLE           1U
#define TFT_ID_HARM_H1              48U
#define TFT_ID_HARM_H2              49U
#define TFT_ID_HARM_H3              50U
#define TFT_ID_HARM_H4              51U
#define TFT_ID_HARM_H5              52U
#define TFT_ID_HARM_THD             53U
#define TFT_ID_HARM_BTN_BACK        54U

/* =========================================================================
 * Public API
 * ========================================================================= */
void   TFT_Init(void);

void   TFT_SwitchPage(uint8_t pageId);
uint8_t TFT_GetCurrentPage(void);

void   TFT_UpdateText(uint8_t pageId, uint16_t controlId, const char *text);
void   TFT_UpdateWaveform(uint8_t pageId, uint16_t controlId,
                          const uint16_t *samples, uint16_t count);

bool   TFT_ReadButton(uint8_t pageId, uint16_t controlId, uint8_t *stateOut);

/* ISR entry — must be invoked from UART_1_INST_IRQHandler.
 * Provided as a function so callers can wire the SysConfig-generated
 * weak handler to it without colliding with our definition. */
void   TFT_UartIsr(void);

#endif /* APP_TFT_H */