#include "app_adc_dma.h"
#include "app_config.h"
#include "app_fft.h"
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

    do {
        App_RunMeasurementFrame();
        Debug_BreakIfEnabled();
    } while (APP_ENABLE_CONTINUOUS_MEASUREMENT != 0);

    while (1) {
        __WFI();
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
