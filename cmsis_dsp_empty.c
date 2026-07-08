#include "app_adc_dma.h"
#include "app_config.h"
#include "app_fft.h"
#include "app_tft.h"
#include "ti_msp_dl_config.h"

#if APP_ENABLE_SYNTHETIC_INPUT
#include "arm_math.h"
#endif

volatile uint16_t *gDebugAdcBuffer;
const float * volatile gDebugFftMag;

volatile uint32_t gDebugPeakBin;
volatile float gDebugPeakFreqHz;
volatile float gDebugPeakAmpAdc;
volatile float gDebugPeakAmpVolt;

volatile uint32_t gDebugBaseBin;
volatile float gDebugBaseFreqHz;
volatile float gDebugTimeFreqHz;

volatile float gDebugTHDPercent;

volatile float gDebugH1Volt;
volatile float gDebugH2Volt;
volatile float gDebugH3Volt;
volatile float gDebugH4Volt;
volatile float gDebugH5Volt;
volatile float gDebugH6Volt;
volatile float gDebugH7Volt;
volatile float gDebugH8Volt;
volatile float gDebugH9Volt;
volatile float gDebugH10Volt;

volatile uint32_t gDebugH1Bin;
volatile uint32_t gDebugH2Bin;
volatile uint32_t gDebugH3Bin;
volatile uint32_t gDebugH4Bin;
volatile uint32_t gDebugH5Bin;
volatile uint32_t gDebugH6Bin;
volatile uint32_t gDebugH7Bin;
volatile uint32_t gDebugH8Bin;
volatile uint32_t gDebugH9Bin;
volatile uint32_t gDebugH10Bin;

volatile uint16_t gDebugAdcMin;
volatile uint16_t gDebugAdcMax;
volatile float gDebugAdcMean;
volatile bool gDebugAdcClipped;

volatile bool gDebugResultValid;
volatile uint32_t gDebugErrorFlags;
volatile uint32_t gDebugFrameCounter;
volatile uint32_t gDebugSampleMode;
volatile bool gDebugAdaptiveLowModeUsed;

static void App_RunMeasurementFrame(void);
static void App_CaptureFrame(void);
static void App_UpdateAdcWatchVariables(volatile uint16_t *adcBuffer);
static bool App_ShouldRetryLowFrequencyMode(void);
static void Debug_UpdateFftWatchVariables(void);
static void Debug_UpdateResultStatus(void);
static void Debug_BreakIfEnabled(void);

static void App_BuildOnePeriodWaveform(uint16_t *out, uint16_t outMax,
                                       uint16_t *outCount);
static void App_RefreshMainPage(void);
static void App_RefreshWaveformPage(void);
static void App_RefreshHarmonicPage(void);
static void App_RunTftStateMachine(void);

#if APP_ENABLE_SYNTHETIC_INPUT
static volatile uint16_t gSyntheticAdcSamples[ADC_SAMPLE_SIZE];
static void App_GenerateSyntheticInput(volatile uint16_t *adcBuffer);
#endif

int main(void)
{
    SYSCFG_DL_init();

#if !APP_ENABLE_SYNTHETIC_INPUT
    ADC_DMA_Init();
#endif

    TFT_Init();
    TFT_SwitchPage(TFT_PAGE_MAIN);

    while (1) {
        App_RunMeasurementFrame();
        Debug_BreakIfEnabled();
        App_RunTftStateMachine();
    }
}

static void App_RunMeasurementFrame(void)
{
    gDebugAdaptiveLowModeUsed = false;

#if APP_ENABLE_SYNTHETIC_INPUT
    App_GenerateSyntheticInput(gSyntheticAdcSamples);
    gDebugAdcBuffer = gSyntheticAdcSamples;
#else
    ADC_DMA_SetSampleMode(ADC_DMA_SAMPLE_MODE_HIGH_FREQ);
    App_CaptureFrame();
#endif

    App_UpdateAdcWatchVariables(gDebugAdcBuffer);
    FFT_Process(gDebugAdcBuffer);

#if !APP_ENABLE_SYNTHETIC_INPUT
#if APP_ENABLE_ADAPTIVE_SAMPLE_MODE
    if (App_ShouldRetryLowFrequencyMode()) {
        ADC_DMA_SetSampleMode(ADC_DMA_SAMPLE_MODE_LOW_FREQ);
        App_CaptureFrame();
        App_UpdateAdcWatchVariables(gDebugAdcBuffer);
        FFT_Process(gDebugAdcBuffer);
        gDebugAdaptiveLowModeUsed = true;
    }
#endif
#endif

    Debug_UpdateFftWatchVariables();
    Debug_UpdateResultStatus();

    gDebugFrameCounter++;
}

static void App_CaptureFrame(void)
{
    ADC_DMA_Start();
    ADC_DMA_WaitDone();
    gDebugAdcBuffer = ADC_DMA_GetBuffer();
}

static void App_UpdateAdcWatchVariables(volatile uint16_t *adcBuffer)
{
    uint16_t adcMin = UINT16_MAX;
    uint16_t adcMax = 0U;
    uint32_t sum = 0U;

    for (uint32_t i = 0U; i < ADC_SAMPLE_SIZE; i++) {
        uint16_t sample = adcBuffer[i];

        if (sample < adcMin) {
            adcMin = sample;
        }

        if (sample > adcMax) {
            adcMax = sample;
        }

        sum += sample;
    }

    gDebugAdcMin = adcMin;
    gDebugAdcMax = adcMax;
    gDebugAdcMean = ((float) sum) / ((float) ADC_SAMPLE_SIZE);
    gDebugAdcClipped =
        (adcMin <= APP_ADC_CLIP_LOW_THRESHOLD) ||
        (adcMax >= APP_ADC_CLIP_HIGH_THRESHOLD);
}

static bool App_ShouldRetryLowFrequencyMode(void)
{
    float measuredFreqHz = FFT_GetTimeDomainFrequencyHz();

    if (measuredFreqHz <= 0.0f) {
        measuredFreqHz = FFT_GetPeakFrequencyHz();
    }

    return (measuredFreqHz > 0.0f) &&
           (measuredFreqHz < APP_ADAPTIVE_LOW_FREQ_THRESHOLD_HZ);
}

static void Debug_UpdateFftWatchVariables(void)
{
    gDebugSampleMode = (uint32_t) ADC_DMA_GetSampleMode();
    gDebugPeakBin = FFT_GetPeakBin();
    gDebugPeakFreqHz = FFT_GetPeakFrequencyHz();
    gDebugPeakAmpAdc = FFT_GetPeakAmplitudeAdc();
    gDebugPeakAmpVolt = FFT_GetPeakAmplitudeVolt();
    gDebugBaseBin = FFT_GetBaseBin();
    gDebugBaseFreqHz = FFT_GetBaseFrequencyHz();
    gDebugTimeFreqHz = FFT_GetTimeDomainFrequencyHz();
    gDebugFftMag = FFT_GetMagnitudeBuffer();
    gDebugTHDPercent = FFT_GetTHDPercent();

    gDebugH1Volt = FFT_GetHarmonicAmplitudeVolt(1);
    gDebugH2Volt = FFT_GetHarmonicAmplitudeVolt(2);
    gDebugH3Volt = FFT_GetHarmonicAmplitudeVolt(3);
    gDebugH4Volt = FFT_GetHarmonicAmplitudeVolt(4);
    gDebugH5Volt = FFT_GetHarmonicAmplitudeVolt(5);
    gDebugH6Volt = FFT_GetHarmonicAmplitudeVolt(6);
    gDebugH7Volt = FFT_GetHarmonicAmplitudeVolt(7);
    gDebugH8Volt = FFT_GetHarmonicAmplitudeVolt(8);
    gDebugH9Volt = FFT_GetHarmonicAmplitudeVolt(9);
    gDebugH10Volt = FFT_GetHarmonicAmplitudeVolt(10);

    gDebugH1Bin = FFT_GetHarmonicBin(1);
    gDebugH2Bin = FFT_GetHarmonicBin(2);
    gDebugH3Bin = FFT_GetHarmonicBin(3);
    gDebugH4Bin = FFT_GetHarmonicBin(4);
    gDebugH5Bin = FFT_GetHarmonicBin(5);
    gDebugH6Bin = FFT_GetHarmonicBin(6);
    gDebugH7Bin = FFT_GetHarmonicBin(7);
    gDebugH8Bin = FFT_GetHarmonicBin(8);
    gDebugH9Bin = FFT_GetHarmonicBin(9);
    gDebugH10Bin = FFT_GetHarmonicBin(10);
}

static void Debug_UpdateResultStatus(void)
{
    uint32_t errorFlags = APP_RESULT_ERROR_NONE;

#if !APP_ENABLE_SYNTHETIC_INPUT
    if (!ADC_DMA_IsDone()) {
        errorFlags |= APP_RESULT_ERROR_DMA_NOT_DONE;
    }
#endif

    if (gDebugAdcClipped) {
        errorFlags |= APP_RESULT_ERROR_ADC_CLIPPED;
    }

    if (!FFT_IsFundamentalValid()) {
        errorFlags |= APP_RESULT_ERROR_FUNDAMENTAL_LOW;
    }

    uint32_t expectedH1Bin = FFT_GetBaseBin();
    uint32_t measuredH1Bin = FFT_GetHarmonicBin(1U);
    uint32_t binTolerance = FFT_GetHarmonicSearchHalfWidth();

    if ((measuredH1Bin + binTolerance < expectedH1Bin) ||
        (measuredH1Bin > expectedH1Bin + binTolerance)) {
        errorFlags |= APP_RESULT_ERROR_H1_BIN_OUT_OF_RANGE;
    }

    gDebugErrorFlags = errorFlags;
    gDebugResultValid = (errorFlags == APP_RESULT_ERROR_NONE);
}

static void Debug_BreakIfEnabled(void)
{
#if APP_ENABLE_DEBUG_BREAK
    __BKPT(0);
#endif
}

#if APP_ENABLE_SYNTHETIC_INPUT
static void App_GenerateSyntheticInput(volatile uint16_t *adcBuffer)
{
    const float dcOffset = 2048.0f;
    const float fundamentalAmp = 800.0f;
    const float h2Ratio = 0.10f;
    const float h3Ratio = 0.05f;
    const float twoPi = 6.28318530718f;
    const float syntheticFreqHz = 1000.0f;
    const float sampleRateHz = ADC_DMA_GetSampleRateHz();

    for (uint32_t i = 0U; i < ADC_SAMPLE_SIZE; i++) {
        float phase = twoPi * syntheticFreqHz * ((float) i) / sampleRateHz;
        float sample =
            dcOffset +
            fundamentalAmp * arm_sin_f32(phase) +
            fundamentalAmp * h2Ratio * arm_sin_f32(2.0f * phase) +
            fundamentalAmp * h3Ratio * arm_sin_f32(3.0f * phase);

        if (sample < 0.0f) {
            sample = 0.0f;
        } else if (sample > APP_ADC_FULL_SCALE) {
            sample = APP_ADC_FULL_SCALE;
        }

        adcBuffer[i] = (uint16_t) (sample + 0.5f);
    }
}
#endif

/* =========================================================================
 * TFT integration: derived measurements + page refresh + state machine
 * ========================================================================= */

/* UTF-8 byte sequences for Chinese labels (best compatibility, no u8"" dep) */
static const uint8_t UTF8_BASE_WAVE[] = {0xE5, 0x9F, 0xBA, 0xE6, 0xB3, 0xA2, 0x00}; /* 基波\0 */
static const uint8_t UTF8_2ND[]      = {0xE4, 0xBA, 0x8C, 0xE6, 0xAC, 0xA1, 0x00}; /* 二次\0 */
static const uint8_t UTF8_3RD[]      = {0xE4, 0xB8, 0x89, 0xE6, 0xAC, 0xA1, 0x00}; /* 三次\0 */
static const uint8_t UTF8_4TH[]      = {0xE5, 0x9B, 0x9B, 0xE6, 0xAC, 0xA1, 0x00}; /* 四次\0 */
static const uint8_t UTF8_5TH[]      = {0xE4, 0xBA, 0x94, 0xE6, 0xAC, 0xA1, 0x00}; /* 五次\0 */
static const uint8_t * const HARM_LABELS[5] = {
    UTF8_BASE_WAVE, UTF8_2ND, UTF8_3RD, UTF8_4TH, UTF8_5TH
};

/* Format helpers — use integer math to avoid snprintf stack usage.
 * Per UART.md, stack-constrained Cortex-M0+ can hard-fault with %f. */
static uint32_t App_FmtThd(char *buf, float thdPercent)
{
    if (thdPercent < 0.0f)   thdPercent = 0.0f;
    if (thdPercent > 999.99f) thdPercent = 999.99f;
    int hundredths = (int)(thdPercent * 100.0f + 0.5f);
    int a = hundredths / 100;
    int b = hundredths % 100;
    buf[0] = (char)('0' + (a / 10) % 10);
    buf[1] = (char)('0' + a % 10);
    buf[2] = '.';
    buf[3] = (char)('0' + (b / 10) % 10);
    buf[4] = (char)('0' + b % 10);
    buf[5] = '%';
    buf[6] = '\0';
    return 6U;
}

static uint32_t App_FmtVolts(char *buf, float volts)
{
    if (volts < 0.0f) volts = -volts;  /* display magnitude */
    if (volts > 99.999f) volts = 99.999f;
    int mV = (int)(volts * 1000.0f + 0.5f);
    int a = mV / 1000;
    int b = mV % 1000;
    buf[0] = (char)('0' + (a / 10) % 10);
    buf[1] = (char)('0' + a % 10);
    buf[2] = '.';
    buf[3] = (char)('0' + (b / 100) % 10);
    buf[4] = (char)('0' + (b / 10) % 10);
    buf[5] = (char)('0' + b % 10);
    buf[6] = ' ';
    buf[7] = 'V';
    buf[8] = '\0';
    return 8U;
}

static uint32_t App_FmtFreq(char *buf, float hz)
{
    if (hz < 0.0f)    hz = 0.0f;
    if (hz > 999999.0f) hz = 999999.0f;
    uint32_t n;
    if (hz >= 1000.0f) {
        /* "X.XXXkHz" — show 4 decimals of kHz */
        int kHzMilli = (int)(hz + 0.5f);  /* Hz rounded */
        int whole = kHzMilli / 1000;
        int frac  = kHzMilli % 1000;
        buf[0] = (char)('0' + (whole / 10) % 10);
        buf[1] = (char)('0' + whole % 10);
        buf[2] = '.';
        buf[3] = (char)('0' + (frac / 100) % 10);
        buf[4] = (char)('0' + (frac / 10) % 10);
        buf[5] = (char)('0' + frac % 10);
        buf[6] = 'k';
        buf[7] = 'H';
        buf[8] = 'z';
        buf[9] = '\0';
        n = 9U;
    } else {
        int wholeHz = (int)(hz + 0.5f);
        if (wholeHz > 9999) wholeHz = 9999;
        int a = wholeHz / 1000;
        int b = (wholeHz / 100) % 10;
        int c = (wholeHz / 10) % 10;
        int d = wholeHz % 10;
        buf[0] = (char)('0' + a % 10);
        buf[1] = (char)('0' + b);
        buf[2] = (char)('0' + c);
        buf[3] = (char)('0' + d);
        buf[4] = '.';
        buf[5] = '0';
        buf[6] = 'H';
        buf[7] = 'z';
        buf[8] = '\0';
        n = 8U;
    }
    return n;
}

static uint32_t App_FmtPeriod(char *buf, float seconds)
{
    if (seconds <= 0.0f) {
        const char *na = "n/a";
        for (uint32_t i = 0; na[i] != '\0'; i++) buf[i] = na[i];
        buf[3] = '\0';
        return 3U;
    }
    if (seconds >= 1.0f) {
        /* "X.XXs" — 2 decimals */
        int centi = (int)(seconds * 100.0f + 0.5f);
        if (centi > 99999) centi = 99999;
        int a = centi / 100;
        int b = centi % 100;
        buf[0] = (char)('0' + (a / 10) % 10);
        buf[1] = (char)('0' + a % 10);
        buf[2] = '.';
        buf[3] = (char)('0' + (b / 10) % 10);
        buf[4] = (char)('0' + b % 10);
        buf[5] = 's';
        buf[6] = '\0';
        return 6U;
    } else {
        /* "<NNN>ms" */
        int ms = (int)(seconds * 1000.0f + 0.5f);
        if (ms > 999) ms = 999;
        if (ms < 1)   ms = 1;
        int a = ms / 100;
        int b = (ms / 10) % 10;
        int c = ms % 10;
        buf[0] = (char)('0' + a);
        buf[1] = (char)('0' + b);
        buf[2] = (char)('0' + c);
        buf[3] = 'm';
        buf[4] = 's';
        buf[5] = '\0';
        return 5U;
    }
}

/* Format: "1.0000 <label>\0" — 4 decimals + UTF-8 label, no %.f */
static uint32_t App_FmtHarmonic(char *buf, float ratio, const uint8_t *label)
{
    if (ratio < 0.0f)    ratio = 0.0f;
    if (ratio > 9.9999f) ratio = 9.9999f;
    int tenThou = (int)(ratio * 10000.0f + 0.5f);
    if (tenThou > 99999) tenThou = 99999;
    int whole   = tenThou / 10000;
    int frac    = tenThou % 10000;
    buf[0] = (char)('0' + whole);
    buf[1] = '.';
    buf[2] = (char)('0' + (frac / 1000) % 10);
    buf[3] = (char)('0' + (frac / 100) % 10);
    buf[4] = (char)('0' + (frac / 10) % 10);
    buf[5] = (char)('0' + frac % 10);
    buf[6] = ' ';
    uint32_t j = 7U;
    if (label != NULL) {
        for (uint32_t i = 0; label[i] != 0U && j < 24U; i++) {
            buf[j++] = (char) label[i];
        }
    }
    buf[j] = '\0';
    return j;
}

static void App_BuildOnePeriodWaveform(uint16_t *out, uint16_t outMax,
                                       uint16_t *outCount)
{
    if (out == NULL || outCount == NULL || outMax == 0U) {
        if (outCount != NULL) *outCount = 0U;
        return;
    }
    float f0 = gDebugPeakFreqHz;
    if (f0 <= 1.0f) {
        *outCount = 0U;
        return;
    }
    float fs = ADC_DMA_GetSampleRateHz();
    if (fs <= 0.0f) {
        *outCount = 0U;
        return;
    }
    uint32_t periodSamples = (uint32_t)(fs / f0 + 0.5f);
    if (periodSamples < 4U) periodSamples = 4U;
    if (periodSamples > outMax) periodSamples = outMax;
    if (periodSamples >= ADC_SAMPLE_SIZE) periodSamples = ADC_SAMPLE_SIZE - 1U;

    /* Center waveform at midpoint of ADC range using gDebugAdcMean */
    float mean = gDebugAdcMean;
    volatile uint16_t *src = gDebugAdcBuffer;
    for (uint32_t i = 0U; i < periodSamples; i++) {
        float centered = ((float) src[i]) - mean + 2047.0f;
        if (centered < 0.0f)         centered = 0.0f;
        if (centered > 4095.0f)      centered = 4095.0f;
        out[i] = (uint16_t)(centered + 0.5f);
    }
    *outCount = (uint16_t) periodSamples;
}

static void App_RefreshMainPage(void)
{
    char buf[24];

    App_FmtThd(buf, gDebugTHDPercent);
    TFT_UpdateText(TFT_PAGE_MAIN, TFT_ID_MAIN_THD, buf);

    float vpp = ((float) gDebugAdcMax - (float) gDebugAdcMin)
                * APP_ADC_REF_VOLTAGE / APP_ADC_FULL_SCALE;
    App_FmtVolts(buf, vpp);
    TFT_UpdateText(TFT_PAGE_MAIN, TFT_ID_MAIN_VPP, buf);

    App_FmtFreq(buf, gDebugPeakFreqHz);
    TFT_UpdateText(TFT_PAGE_MAIN, TFT_ID_MAIN_F0, buf);

    float period = (gDebugPeakFreqHz > 1.0f) ? (1.0f / gDebugPeakFreqHz) : 0.0f;
    App_FmtPeriod(buf, period);
    TFT_UpdateText(TFT_PAGE_MAIN, TFT_ID_MAIN_T, buf);
}

static void App_RefreshWaveformPage(void)
{
    char buf[24];
    float mean = gDebugAdcMean;

    float vmax = ((float) gDebugAdcMax - mean)
                 * APP_ADC_REF_VOLTAGE / APP_ADC_FULL_SCALE;
    float vmin = ((float) gDebugAdcMin - mean)
                 * APP_ADC_REF_VOLTAGE / APP_ADC_FULL_SCALE;
    float vpp = vmax - vmin;

    /* Prepend + or - sign for Vmax/Vmin */
    char vmaxBuf[16];
    char vminBuf[16];
    App_FmtVolts(vmaxBuf, vmax);
    App_FmtVolts(vminBuf, vmin);
    buf[0] = '+';
    for (uint32_t i = 0; i < 9U && vmaxBuf[i] != '\0'; i++) buf[i + 1U] = vmaxBuf[i];
    TFT_UpdateText(TFT_PAGE_WAVEFORM, TFT_ID_WAVE_VMAX, buf);

    buf[0] = '-';
    for (uint32_t i = 0; i < 9U && vminBuf[i] != '\0'; i++) buf[i + 1U] = vminBuf[i];
    TFT_UpdateText(TFT_PAGE_WAVEFORM, TFT_ID_WAVE_VMIN, buf);

    App_FmtVolts(buf, vpp);
    TFT_UpdateText(TFT_PAGE_WAVEFORM, TFT_ID_WAVE_VPP, buf);

    /* Waveform graph */
    uint16_t wave[TFT_WAVE_MAX_POINTS];
    uint16_t n = 0U;
    App_BuildOnePeriodWaveform(wave, TFT_WAVE_MAX_POINTS, &n);
    if (n > 0U) {
        TFT_UpdateWaveform(TFT_PAGE_WAVEFORM, TFT_ID_WAVE_GRAPH, wave, n);
    }
}

static void App_RefreshHarmonicPage(void)
{
    char buf[32];
    float h1 = gDebugH1Volt;
    float harmonic[5];
    harmonic[0] = gDebugH1Volt;
    harmonic[1] = gDebugH2Volt;
    harmonic[2] = gDebugH3Volt;
    harmonic[3] = gDebugH4Volt;
    harmonic[4] = gDebugH5Volt;

    uint16_t ids[5] = {
        TFT_ID_HARM_H1, TFT_ID_HARM_H2, TFT_ID_HARM_H3,
        TFT_ID_HARM_H4, TFT_ID_HARM_H5
    };

    for (uint32_t i = 0; i < 5U; i++) {
        float ratio = (h1 > 0.001f) ? (harmonic[i] / h1) : 0.0f;
        App_FmtHarmonic(buf, ratio, HARM_LABELS[i]);
        TFT_UpdateText(TFT_PAGE_HARMONIC, ids[i], buf);
    }

    App_FmtThd(buf, gDebugTHDPercent);
    TFT_UpdateText(TFT_PAGE_HARMONIC, TFT_ID_HARM_THD, buf);
}

static void App_RunTftStateMachine(void)
{
    uint8_t bs;

    switch (TFT_GetCurrentPage()) {
    case TFT_PAGE_MAIN:
        App_RefreshMainPage();
        if (TFT_ReadButton(TFT_PAGE_MAIN, TFT_ID_MAIN_BTN_WAVEFORM, &bs) && bs == TFT_BUTTON_PRESSED) {
            TFT_SwitchPage(TFT_PAGE_WAVEFORM);
        } else if (TFT_ReadButton(TFT_PAGE_MAIN, TFT_ID_MAIN_BTN_HARMONIC, &bs) && bs == TFT_BUTTON_PRESSED) {
            TFT_SwitchPage(TFT_PAGE_HARMONIC);
        }
        /* BTN_MEASURE (16): no-op — measurement runs continuously */
        break;

    case TFT_PAGE_WAVEFORM:
        App_RefreshWaveformPage();
        if (TFT_ReadButton(TFT_PAGE_WAVEFORM, TFT_ID_WAVE_BTN_BACK, &bs) && bs == TFT_BUTTON_PRESSED) {
            TFT_SwitchPage(TFT_PAGE_MAIN);
        }
        break;

    case TFT_PAGE_HARMONIC:
        App_RefreshHarmonicPage();
        if (TFT_ReadButton(TFT_PAGE_HARMONIC, TFT_ID_HARM_BTN_BACK, &bs) && bs == TFT_BUTTON_PRESSED) {
            TFT_SwitchPage(TFT_PAGE_MAIN);
        }
        break;

    default:
        /* Unknown page; reset to main */
        TFT_SwitchPage(TFT_PAGE_MAIN);
        break;
    }
}
