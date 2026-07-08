#include "app_tft.h"
#include "ti_msp_dl_config.h"
#include <string.h>

/* =========================================================================
 * Module-private state
 * ========================================================================= */
static volatile uint8_t sRxRing[TFT_RX_RING_SIZE];
static volatile uint16_t sRxHead;
static volatile uint16_t sRxTail;
static volatile uint8_t  sButtonState[TFT_BUTTON_TABLE_SIZE];

static uint8_t sCurrentPage = 0xFFU;

/* =========================================================================
 * Forward declarations
 * ========================================================================= */
static void TFT_TxByte(uint8_t b);
static void TFT_TxFrame(uint8_t widgetClass, uint8_t subCmd,
                        const uint8_t *data, uint16_t dataLen);
static uint32_t TFT_EncodePageAndId(uint8_t *out, uint8_t pageId, uint16_t controlId);
static void TFT_RingPut(uint8_t b);
static bool TFT_RingGet(uint8_t *b);
static uint16_t TFT_RingCount(void);
static void TFT_ParseRxFrames(void);
static void TFT_RequestButtonState(uint8_t pageId, uint16_t controlId);

/* =========================================================================
 * TX: byte-level transmit with inter-byte pacing
 * ========================================================================= */
static void TFT_TxByte(uint8_t b)
{
    DL_UART_Main_transmitData(UART_1_INST, b);
    delay_cycles(TFT_TX_BYTE_DELAY_CYCLES);
}

static void TFT_TxFrame(uint8_t widgetClass, uint8_t subCmd,
                        const uint8_t *data, uint16_t dataLen)
{
    uint8_t hdr[3];

    hdr[0] = TFT_FRAME_HEAD0;
    hdr[1] = widgetClass;
    hdr[2] = subCmd;

    for (uint8_t i = 0; i < 3U; i++) {
        TFT_TxByte(hdr[i]);
    }
    for (uint16_t i = 0; i < dataLen; i++) {
        TFT_TxByte(data[i]);
    }
    TFT_TxByte(TFT_FRAME_TAIL0);
    TFT_TxByte(TFT_FRAME_TAIL1);
    TFT_TxByte(TFT_FRAME_TAIL2);
    TFT_TxByte(TFT_FRAME_TAIL3);

    /* Let the screen begin processing before the next frame */
    delay_cycles(TFT_TX_BYTE_DELAY_CYCLES * 2U);
}

/* Helper: encode page_id (2 bytes BE) + control_id (2 bytes BE) into a buffer.
 * Returns number of bytes written (always 4). */
static uint32_t TFT_EncodePageAndId(uint8_t *out, uint8_t pageId, uint16_t controlId)
{
    out[0] = 0x00U;                       /* page hi */
    out[1] = pageId;                      /* page lo */
    out[2] = (uint8_t)((controlId >> 8) & 0xFFU);
    out[3] = (uint8_t)(controlId & 0xFFU);
    return 4U;
}

/* =========================================================================
 * RX ring buffer (single-byte put/get)
 * ========================================================================= */
static void TFT_RingPut(uint8_t b)
{
    uint16_t next = (uint16_t)((sRxHead + 1U) % TFT_RX_RING_SIZE);
    if (next != sRxTail) {
        sRxRing[sRxHead] = b;
        sRxHead = next;
    }
    /* Else: drop on overflow (the parser will catch up next call) */
}

static bool TFT_RingGet(uint8_t *b)
{
    if (sRxHead == sRxTail) {
        return false;
    }
    *b = sRxRing[sRxTail];
    sRxTail = (uint16_t)((sRxTail + 1U) % TFT_RX_RING_SIZE);
    return true;
}

static uint16_t TFT_RingCount(void)
{
    return (uint16_t)((sRxHead - sRxTail) % TFT_RX_RING_SIZE);
}

/* =========================================================================
 * RX frame parser: walks ring buffer looking for 0xEE ... 0xFF FC FF FF
 * Frame structure (no LEN byte): [0xEE] [WIDGET] [SUBCMD] [DATA...] [0xFF FC FF FF]
 * Drops malformed frames silently.
 * ========================================================================= */
static void TFT_ParseRxFrames(void)
{
    while (TFT_RingCount() >= 9U) {
        /* Peek for 0xEE */
        uint16_t peekIdx = sRxTail;
        if (sRxRing[peekIdx] != TFT_FRAME_HEAD0) {
            uint8_t junk;
            (void) TFT_RingGet(&junk);
            continue;
        }

        /* Look for frame tail 0xFF FC FF FF after the 3-byte header
         * (header + widget_class + subcmd). Search from index 3 onward. */
        uint16_t total = TFT_RingCount();
        uint16_t tailStart = 0xFFFFU;
        for (uint16_t i = 3U; i + 4U <= total; i++) {
            uint16_t idx = (uint16_t)((sRxTail + i) % TFT_RX_RING_SIZE);
            if (sRxRing[idx] == TFT_FRAME_TAIL0 &&
                sRxRing[(uint16_t)((idx + 1U) % TFT_RX_RING_SIZE)] == TFT_FRAME_TAIL1 &&
                sRxRing[(uint16_t)((idx + 2U) % TFT_RX_RING_SIZE)] == TFT_FRAME_TAIL2 &&
                sRxRing[(uint16_t)((idx + 3U) % TFT_RX_RING_SIZE)] == TFT_FRAME_TAIL3) {
                tailStart = idx;
                break;
            }
        }

        if (tailStart == 0xFFFFU) {
            /* Incomplete frame in buffer; wait for more data */
            break;
        }

        /* Compute dataLen from the tail position (no LEN byte in protocol).
         *   bytes 0..2    = 0xEE, widget, subcmd
         *   bytes 3..n    = data
         *   bytes n..n+3  = 0xFF FC FF FF (tail) */
        uint16_t headerLen = 3U;
        uint16_t dataLen;
        if (tailStart >= sRxTail) {
            dataLen = (uint16_t)(tailStart - sRxTail - headerLen);
        } else {
            /* Wrapped around ring */
            dataLen = (uint16_t)((TFT_RX_RING_SIZE - sRxTail - headerLen) + tailStart);
        }

        /* Sanity-cap data length to avoid reading garbage if tail was spurious */
        if (dataLen > 16U) dataLen = 16U;

        /* Consume header bytes (0xEE, widget_class, subcmd) */
        uint8_t b;
        (void) TFT_RingGet(&b);  /* 0xEE */
        (void) TFT_RingGet(&b);  /* widget_class */
        uint8_t widgetClass = b;
        (void) TFT_RingGet(&b);  /* subcmd */
        uint8_t subCmd = b;

        /* Read data bytes */
        uint8_t data[16];
        for (uint16_t i = 0; i < dataLen; i++) {
            (void) TFT_RingGet(&data[i]);
        }

        /* Consume tail (4 bytes) */
        for (uint8_t i = 0; i < 4U; i++) {
            (void) TFT_RingGet(&b);
        }

        /* Handle button-state response:
         *   data = [page_hi, page_lo, id_hi, id_lo, state] */
        if (widgetClass == TFT_WIDGET_BUTTON && subCmd == TFT_SUBCMD_READ &&
            dataLen >= 5U) {
            uint16_t id = (uint16_t)((data[2] << 8) | data[3]);
            uint8_t  st = data[4];
            if (id < TFT_BUTTON_TABLE_SIZE) {
                sButtonState[id] = st;
            }
        }
        /* Other response types are ignored for now */
    }
}

/* Send a button-state read request to the screen */
static void TFT_RequestButtonState(uint8_t pageId, uint16_t controlId)
{
    uint8_t data[4];
    /* Format: [page_hi] [page_lo] [id_hi] [id_lo] */
    TFT_EncodePageAndId(data, pageId, controlId);
    TFT_TxFrame(TFT_WIDGET_BUTTON, TFT_SUBCMD_READ, data, 4U);
}

/* =========================================================================
 * ISR handler: drain RX FIFO into ring buffer
 * SysConfig generates `UART_1_INST_IRQHandler` in ti_msp_dl_config.c ONLY
 * if it is marked weak. Since we found no definition in ti_msp_dl_config.c,
 * we define the handler here and rely on the link-time alias
 * `UART_1_INST_IRQHandler -> UART1_IRQHandler`.
 * ========================================================================= */
void UART_1_INST_IRQHandler(void)
{
    uint32_t status = DL_UART_Main_getPendingInterrupt(UART_1_INST);
    if ((status & DL_UART_IIDX_RX) == DL_UART_IIDX_RX) {
        while (DL_UART_Main_isRXFIFOEmpty(UART_1_INST) == false) {
            uint8_t b = (uint8_t) DL_UART_Main_receiveData(UART_1_INST);
            TFT_RingPut(b);
        }
    }
    /* TX interrupt (if enabled) is not used in this design. */
}

/* Optional alias for symmetry with app_tft.h API */
void TFT_UartIsr(void)
{
    /* The macro UART_1_INST_IRQHandler already aliases UART1_IRQHandler.
     * This function is unused; the real handler is UART_1_INST_IRQHandler. */
}

/* =========================================================================
 * Public API
 * ========================================================================= */
void TFT_Init(void)
{
    sRxHead = 0U;
    sRxTail = 0U;
    for (uint32_t i = 0; i < TFT_BUTTON_TABLE_SIZE; i++) {
        sButtonState[i] = TFT_BUTTON_RELEASED;
    }
    sCurrentPage = 0xFFU;

    /* Clear any stale RX FIFO contents */
    while (DL_UART_Main_isRXFIFOEmpty(UART_1_INST) == false) {
        (void) DL_UART_Main_receiveData(UART_1_INST);
    }

    /* RX FIFO threshold interrupt is enabled in SysConfig.
     * Just make sure NVIC is on. */
    NVIC_EnableIRQ(UART_1_INST_INT_IRQN);
}

void TFT_SwitchPage(uint8_t pageId)
{
    /* Page widget: B1 + subcommand, data = page_hi page_lo */
    uint8_t data[2] = { 0x00U, pageId };
    TFT_TxFrame(TFT_WIDGET_PAGE, TFT_SUBCMD_WRITE, data, 2U);
    sCurrentPage = pageId;
}

uint8_t TFT_GetCurrentPage(void)
{
    return sCurrentPage;
}

void TFT_UpdateText(uint8_t pageId, uint16_t controlId, const char *text)
{
    if (text == NULL) return;

    /* Dacai protocol frame data = [page_hi] [page_lo] [id_hi] [id_lo] [text bytes...] */
    uint8_t data[TFT_TX_BUF_SIZE - 7U];  /* leave room for header/tail */
    uint16_t idx = 0U;
    idx += (uint16_t) TFT_EncodePageAndId(data, pageId, controlId);

    /* Copy text bytes until null terminator or buffer full */
    while (text[idx - 4U] != '\0' && idx < (TFT_TX_BUF_SIZE - 7U)) {
        data[idx] = (uint8_t) text[idx - 4U];
        idx++;
    }
    TFT_TxFrame(TFT_WIDGET_TEXT, TFT_SUBCMD_WRITE, data, idx);
}

void TFT_UpdateWaveform(uint8_t pageId, uint16_t controlId,
                        const uint16_t *samples, uint16_t count)
{
    if (samples == NULL || count == 0U) return;
    if (count > TFT_WAVE_MAX_POINTS) count = TFT_WAVE_MAX_POINTS;

    /* Curve frame data = [page_hi] [page_lo] [id_hi] [id_lo] [channel] [n_lo] [n_hi] [y0...] */
    /* Plus 7 bytes for header/tail = 7 + 7 + 2*count total frame */
    if ((uint32_t)(7U + 2U * count) + 7U > TFT_TX_BUF_SIZE) return;

    uint8_t data[TFT_TX_BUF_SIZE - 7U];
    uint16_t idx = 0U;
    idx += (uint16_t) TFT_EncodePageAndId(data, pageId, controlId);
    data[idx++] = (uint8_t) TFT_WAVE_CHANNEL_0;
    data[idx++] = (uint8_t)(count & 0xFFU);
    data[idx++] = (uint8_t)((count >> 8) & 0xFFU);
    for (uint16_t i = 0; i < count; i++) {
        uint16_t v = samples[i];
        data[idx++] = (uint8_t)(v & 0xFFU);
        data[idx++] = (uint8_t)((v >> 8) & 0xFFU);
    }
    TFT_TxFrame(TFT_WIDGET_CURVE, TFT_SUBCMD_WRITE, data, idx);
}

bool TFT_ReadButton(uint8_t pageId, uint16_t controlId, uint8_t *stateOut)
{
    TFT_ParseRxFrames();
    TFT_RequestButtonState(pageId, controlId);  /* ask screen to send current state */

    if (controlId >= TFT_BUTTON_TABLE_SIZE) {
        if (stateOut != NULL) *stateOut = TFT_BUTTON_RELEASED;
        return false;
    }
    uint8_t s = sButtonState[controlId];
    if (stateOut != NULL) *stateOut = s;
    return (s == TFT_BUTTON_PRESSED);
}