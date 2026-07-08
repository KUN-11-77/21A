#include "app_fft.h"
#include "arm_const_structs.h"
#include "arm_math.h"
#include <stddef.h>

#if APP_FFT_SIZE != 2048
#error "This FFT version uses arm_cfft_sR_f32_len2048, so APP_FFT_SIZE must be 2048."
#endif

#define FFT_MIN_FUNDAMENTAL_VOLT 0.001f

/*
 * CMSIS CFFT uses interleaved complex input:
 * real0, imag0, real1, imag1, ...
 */
static float32_t gFFTInput[APP_FFT_SIZE * 2U];
static float32_t gFFTMagnitude[APP_FFT_SIZE / 2U];

static uint32_t gPeakBin = 0U;
static float32_t gPeakAmplitudeAdc = 0.0f;
static float32_t gPeakFrequencyHz = 0.0f;

static uint32_t gBaseBin = 0U;
static float32_t gBaseFrequencyHz = 0.0f;
static float32_t gTimeDomainFrequencyHz = 0.0f;

static float32_t gHarmonicAmpAdc[FFT_HARMONIC_MAX + 1U];
static float32_t gHarmonicAmpVolt[FFT_HARMONIC_MAX + 1U];
static uint32_t gHarmonicBin[FFT_HARMONIC_MAX + 1U];
static float32_t gTHDPercent = 0.0f;

static void FFT_ClearResults(void);
static float32_t FFT_CalculateMean(volatile uint16_t *adcBuffer);
static float32_t FFT_EstimateTimeDomainFrequencyHz(
    volatile uint16_t *adcBuffer,
    float32_t mean);
static void FFT_BuildComplexInput(volatile uint16_t *adcBuffer, float32_t mean);
static void FFT_NormalizeSingleSidedMagnitude(void);
static uint32_t FFT_CeilFreqToBin(float32_t freqHz);
static uint32_t FFT_FloorFreqToBin(float32_t freqHz);
static uint32_t FFT_FindPeakBinInRange(
    uint32_t startBin,
    uint32_t endBin,
    float32_t *peakAmplitudeOut);
static void FFT_FindOverallPeak(void);
static float32_t FFT_EstimatePeakFrequencyHz(uint32_t peakBin);
static void FFT_SelectBaseBin(void);
static uint32_t FFT_GetHarmonicCenterBin(uint32_t harmonic);
static float32_t FFT_GetBandAmplitudeAdc(
    uint32_t centerBin,
    uint32_t width,
    uint32_t *peakBinOut);
static void FFT_CalculateHarmonicsAndTHD(void);

void FFT_Process(volatile uint16_t *adcBuffer)
{
    if (adcBuffer == NULL) {
        FFT_ClearResults();
        return;
    }

    float32_t mean = FFT_CalculateMean(adcBuffer);
    gTimeDomainFrequencyHz = FFT_EstimateTimeDomainFrequencyHz(adcBuffer, mean);

    FFT_BuildComplexInput(adcBuffer, mean);
    arm_cfft_f32(&arm_cfft_sR_f32_len2048, gFFTInput, 0, 1);
    arm_cmplx_mag_f32(gFFTInput, gFFTMagnitude, APP_FFT_SIZE / 2U);

    FFT_NormalizeSingleSidedMagnitude();
    FFT_FindOverallPeak();
    FFT_SelectBaseBin();
    FFT_CalculateHarmonicsAndTHD();
}

const float *FFT_GetMagnitudeBuffer(void)
{
    return gFFTMagnitude;
}

uint32_t FFT_GetPeakBin(void)
{
    return gPeakBin;
}

float FFT_GetPeakFrequencyHz(void)
{
    return gPeakFrequencyHz;
}

float FFT_GetPeakAmplitudeAdc(void)
{
    return gPeakAmplitudeAdc;
}

float FFT_GetPeakAmplitudeVolt(void)
{
    return FFT_AdcAmplitudeToVolt(gPeakAmplitudeAdc);
}

uint32_t FFT_GetBaseBin(void)
{
    return gBaseBin;
}

float FFT_GetBaseFrequencyHz(void)
{
    return gBaseFrequencyHz;
}

float FFT_GetTimeDomainFrequencyHz(void)
{
    return gTimeDomainFrequencyHz;
}

float FFT_GetBinFrequencyHz(uint32_t bin)
{
    return ((float32_t) bin) * ADC_DMA_GetSampleRateHz() / ((float32_t) APP_FFT_SIZE);
}

float FFT_GetBinAmplitudeAdc(uint32_t bin)
{
    if (bin >= (APP_FFT_SIZE / 2U)) {
        return 0.0f;
    }

    return gFFTMagnitude[bin];
}

float FFT_GetBinAmplitudeVolt(uint32_t bin)
{
    return FFT_AdcAmplitudeToVolt(FFT_GetBinAmplitudeAdc(bin));
}

float FFT_GetTHDPercent(void)
{
    return gTHDPercent;
}

bool FFT_IsFundamentalValid(void)
{
    return gHarmonicAmpVolt[1U] > FFT_MIN_FUNDAMENTAL_VOLT;
}

float FFT_GetHarmonicAmplitudeVolt(uint32_t harmonic)
{
    if (harmonic > FFT_HARMONIC_MAX) {
        return 0.0f;
    }

    return gHarmonicAmpVolt[harmonic];
}

float FFT_GetHarmonicAmplitudeAdc(uint32_t harmonic)
{
    if (harmonic > FFT_HARMONIC_MAX) {
        return 0.0f;
    }

    return gHarmonicAmpAdc[harmonic];
}

uint32_t FFT_GetHarmonicBin(uint32_t harmonic)
{
    if (harmonic > FFT_HARMONIC_MAX) {
        return 0U;
    }

    return gHarmonicBin[harmonic];
}

uint32_t FFT_GetExpectedHarmonicBin(uint32_t harmonic)
{
    if ((harmonic == 0U) || (harmonic > FFT_HARMONIC_MAX)) {
        return 0U;
    }

    return gBaseBin * harmonic;
}

uint32_t FFT_GetHarmonicSearchHalfWidth(void)
{
    return FFT_SEARCH_HALF_WIDTH;
}

float FFT_AdcAmplitudeToVolt(float adcAmplitude)
{
    return adcAmplitude * APP_ADC_REF_VOLTAGE / APP_ADC_FULL_SCALE;
}

static void FFT_ClearResults(void)
{
    gPeakBin = 0U;
    gPeakAmplitudeAdc = 0.0f;
    gPeakFrequencyHz = 0.0f;
    gBaseBin = 0U;
    gBaseFrequencyHz = 0.0f;
    gTimeDomainFrequencyHz = 0.0f;
    gTHDPercent = 0.0f;

    for (uint32_t i = 0U; i < (APP_FFT_SIZE / 2U); i++) {
        gFFTMagnitude[i] = 0.0f;
    }

    for (uint32_t h = 0U; h <= FFT_HARMONIC_MAX; h++) {
        gHarmonicBin[h] = 0U;
        gHarmonicAmpAdc[h] = 0.0f;
        gHarmonicAmpVolt[h] = 0.0f;
    }
}

static float32_t FFT_CalculateMean(volatile uint16_t *adcBuffer)
{
    float32_t sum = 0.0f;

    for (uint32_t i = 0U; i < APP_FFT_SIZE; i++) {
        sum += (float32_t) adcBuffer[i];
    }

    return sum / ((float32_t) APP_FFT_SIZE);
}

static float32_t FFT_EstimateTimeDomainFrequencyHz(
    volatile uint16_t *adcBuffer,
    float32_t mean)
{
    uint16_t adcMin = UINT16_MAX;
    uint16_t adcMax = 0U;

    for (uint32_t i = 0U; i < APP_FFT_SIZE; i++) {
        uint16_t sample = adcBuffer[i];

        if (sample < adcMin) {
            adcMin = sample;
        }

        if (sample > adcMax) {
            adcMax = sample;
        }
    }

    float32_t peakToPeak = ((float32_t) adcMax) - ((float32_t) adcMin);

    if (peakToPeak < 16.0f) {
        return 0.0f;
    }

    float32_t hysteresis = peakToPeak * 0.05f;

    if (hysteresis < 8.0f) {
        hysteresis = 8.0f;
    }

    float32_t lowThreshold = mean - hysteresis;
    float32_t highThreshold = mean + hysteresis;
    bool armed = (((float32_t) adcBuffer[0]) < lowThreshold);
    float32_t firstCross = 0.0f;
    float32_t lastCross = 0.0f;
    uint32_t crossingCount = 0U;
    float32_t prevSample = (float32_t) adcBuffer[0];

    for (uint32_t i = 1U; i < APP_FFT_SIZE; i++) {
        float32_t sample = (float32_t) adcBuffer[i];

        if (sample < lowThreshold) {
            armed = true;
        } else if ((armed == true) && (sample >= highThreshold)) {
            float32_t denominator = sample - prevSample;
            float32_t fraction = 0.0f;

            if (denominator > 0.0f) {
                fraction = (highThreshold - prevSample) / denominator;

                if (fraction < 0.0f) {
                    fraction = 0.0f;
                } else if (fraction > 1.0f) {
                    fraction = 1.0f;
                }
            }

            float32_t cross = ((float32_t) (i - 1U)) + fraction;

            if (crossingCount == 0U) {
                firstCross = cross;
            }

            lastCross = cross;
            crossingCount++;
            armed = false;
        }

        prevSample = sample;
    }

    if (crossingCount < 2U) {
        return 0.0f;
    }

    float32_t samplePeriods = lastCross - firstCross;

    if (samplePeriods <= 0.0f) {
        return 0.0f;
    }

    return (((float32_t) (crossingCount - 1U)) * ADC_DMA_GetSampleRateHz()) /
           samplePeriods;
}

static void FFT_BuildComplexInput(volatile uint16_t *adcBuffer, float32_t mean)
{
    for (uint32_t i = 0U; i < APP_FFT_SIZE; i++) {
        gFFTInput[2U * i] = ((float32_t) adcBuffer[i]) - mean;
        gFFTInput[(2U * i) + 1U] = 0.0f;
    }
}

static void FFT_NormalizeSingleSidedMagnitude(void)
{
    gFFTMagnitude[0] = 0.0f;

    for (uint32_t k = 1U; k < (APP_FFT_SIZE / 2U); k++) {
        gFFTMagnitude[k] = 2.0f * gFFTMagnitude[k] / ((float32_t) APP_FFT_SIZE);
    }
}

static uint32_t FFT_CeilFreqToBin(float32_t freqHz)
{
    float32_t binFloat =
        freqHz * ((float32_t) APP_FFT_SIZE) / ADC_DMA_GetSampleRateHz();
    uint32_t bin = (uint32_t) binFloat;

    if (((float32_t) bin) < binFloat) {
        bin++;
    }

    return bin;
}

static uint32_t FFT_FloorFreqToBin(float32_t freqHz)
{
    float32_t binFloat =
        freqHz * ((float32_t) APP_FFT_SIZE) / ADC_DMA_GetSampleRateHz();

    return (uint32_t) binFloat;
}

static uint32_t FFT_FindPeakBinInRange(
    uint32_t startBin,
    uint32_t endBin,
    float32_t *peakAmplitudeOut)
{
    uint32_t nyquistBin = (APP_FFT_SIZE / 2U);

    if (startBin == 0U) {
        startBin = 1U;
    }

    if (endBin >= nyquistBin) {
        endBin = nyquistBin - 1U;
    }

    if (startBin > endBin) {
        if (peakAmplitudeOut != NULL) {
            *peakAmplitudeOut = 0.0f;
        }

        return 0U;
    }

    uint32_t peakBin = startBin;
    float32_t peakAmp = gFFTMagnitude[startBin];

    for (uint32_t k = startBin + 1U; k <= endBin; k++) {
        if (gFFTMagnitude[k] > peakAmp) {
            peakAmp = gFFTMagnitude[k];
            peakBin = k;
        }
    }

    if (peakAmplitudeOut != NULL) {
        *peakAmplitudeOut = peakAmp;
    }

    return peakBin;
}

static void FFT_FindOverallPeak(void)
{
    uint32_t startBin = FFT_CeilFreqToBin(FFT_MIN_BASE_FREQ_HZ);
    uint32_t endBin = FFT_FloorFreqToBin(FFT_MAX_BASE_FREQ_HZ);

    gPeakBin = FFT_FindPeakBinInRange(startBin, endBin, &gPeakAmplitudeAdc);
    gPeakFrequencyHz = FFT_EstimatePeakFrequencyHz(gPeakBin);

    if ((gTimeDomainFrequencyHz >= FFT_MIN_BASE_FREQ_HZ) &&
        (gTimeDomainFrequencyHz <= FFT_MAX_BASE_FREQ_HZ)) {
        gPeakFrequencyHz = gTimeDomainFrequencyHz;
    }
}

static float32_t FFT_EstimatePeakFrequencyHz(uint32_t peakBin)
{
    if ((peakBin == 0U) || (peakBin >= ((APP_FFT_SIZE / 2U) - 1U))) {
        return FFT_GetBinFrequencyHz(peakBin);
    }

    float32_t left = gFFTMagnitude[peakBin - 1U];
    float32_t center = gFFTMagnitude[peakBin];
    float32_t right = gFFTMagnitude[peakBin + 1U];
    float32_t denominator = (left - (2.0f * center)) + right;
    float32_t offset = 0.0f;

    if (denominator != 0.0f) {
        offset = 0.5f * (left - right) / denominator;

        if (offset < -0.5f) {
            offset = -0.5f;
        } else if (offset > 0.5f) {
            offset = 0.5f;
        }
    }

    return (((float32_t) peakBin) + offset) *
           ADC_DMA_GetSampleRateHz() / ((float32_t) APP_FFT_SIZE);
}

static void FFT_SelectBaseBin(void)
{
#if FFT_USE_AUTO_BASE_BIN
    if ((gTimeDomainFrequencyHz >= FFT_MIN_BASE_FREQ_HZ) &&
        (gTimeDomainFrequencyHz <= FFT_MAX_BASE_FREQ_HZ)) {
        float32_t baseBinFloat =
            gTimeDomainFrequencyHz * ((float32_t) APP_FFT_SIZE) /
            ADC_DMA_GetSampleRateHz();
        uint32_t nominalBaseBin = (uint32_t) (baseBinFloat + 0.5f);
        if (nominalBaseBin >= (APP_FFT_SIZE / 2U)) {
            nominalBaseBin = (APP_FFT_SIZE / 2U) - 1U;
        } else if (nominalBaseBin == 0U) {
            nominalBaseBin = 1U;
        }

        gBaseBin = nominalBaseBin;
        gBaseFrequencyHz = gTimeDomainFrequencyHz;
        return;
    }

    gBaseBin = gPeakBin;
    gBaseFrequencyHz = FFT_GetBinFrequencyHz(gBaseBin);
#else
    float32_t nominalBaseBinFloat =
        FFT_BASE_FREQ_HZ * ((float32_t) APP_FFT_SIZE) / ADC_DMA_GetSampleRateHz();
    uint32_t nominalBaseBin = (uint32_t) (nominalBaseBinFloat + 0.5f);
    uint32_t startBin = 1U;
    uint32_t endBin = nominalBaseBin + FFT_SEARCH_HALF_WIDTH;

    if (nominalBaseBin > FFT_SEARCH_HALF_WIDTH) {
        startBin = nominalBaseBin - FFT_SEARCH_HALF_WIDTH;
    }

    gBaseBin = FFT_FindPeakBinInRange(startBin, endBin, NULL);
    gBaseFrequencyHz = FFT_GetBinFrequencyHz(gBaseBin);
#endif
}

static uint32_t FFT_GetHarmonicCenterBin(uint32_t harmonic)
{
    float32_t centerBinFloat =
        gBaseFrequencyHz * ((float32_t) harmonic) *
        ((float32_t) APP_FFT_SIZE) / ADC_DMA_GetSampleRateHz();
    uint32_t centerBin = (uint32_t) (centerBinFloat + 0.5f);

    if (centerBin >= (APP_FFT_SIZE / 2U)) {
        return APP_FFT_SIZE / 2U;
    }

    return centerBin;
}

static float32_t FFT_GetBandAmplitudeAdc(
    uint32_t centerBin,
    uint32_t width,
    uint32_t *peakBinOut)
{
    uint32_t startBin = 1U;
    uint32_t endBin = centerBin + width;

    if (centerBin > width) {
        startBin = centerBin - width;
    }

    if (endBin >= (APP_FFT_SIZE / 2U)) {
        endBin = (APP_FFT_SIZE / 2U) - 1U;
    }

    float32_t sumSq = 0.0f;
    uint32_t peakBin = startBin;
    float32_t peakAmp = gFFTMagnitude[startBin];

    for (uint32_t k = startBin; k <= endBin; k++) {
        float32_t amp = gFFTMagnitude[k];

        sumSq += amp * amp;

        if (amp > peakAmp) {
            peakAmp = amp;
            peakBin = k;
        }
    }

    if (peakBinOut != NULL) {
        *peakBinOut = peakBin;
    }

    float32_t bandAmplitude = 0.0f;
    arm_sqrt_f32(sumSq, &bandAmplitude);

    return bandAmplitude;
}

static void FFT_CalculateHarmonicsAndTHD(void)
{
    for (uint32_t h = 1U; h <= FFT_HARMONIC_MAX; h++) {
        uint32_t centerBin = FFT_GetHarmonicCenterBin(h);

        if ((centerBin == 0U) || (centerBin >= (APP_FFT_SIZE / 2U))) {
            gHarmonicBin[h] = 0U;
            gHarmonicAmpAdc[h] = 0.0f;
            gHarmonicAmpVolt[h] = 0.0f;
            continue;
        }

        uint32_t peakBin = 0U;
        gHarmonicAmpAdc[h] =
            FFT_GetBandAmplitudeAdc(centerBin, FFT_SEARCH_HALF_WIDTH, &peakBin);
        gHarmonicAmpVolt[h] = FFT_AdcAmplitudeToVolt(gHarmonicAmpAdc[h]);
        gHarmonicBin[h] = peakBin;
    }

    float32_t v1 = gHarmonicAmpVolt[1U];

    if (v1 <= FFT_MIN_FUNDAMENTAL_VOLT) {
        gTHDPercent = 0.0f;
        return;
    }

    float32_t harmonicPower = 0.0f;

    for (uint32_t h = 2U; h <= FFT_HARMONIC_MAX; h++) {
        harmonicPower += gHarmonicAmpVolt[h] * gHarmonicAmpVolt[h];
    }

    float32_t harmonicRms = 0.0f;
    arm_sqrt_f32(harmonicPower, &harmonicRms);

    gTHDPercent = harmonicRms / v1 * 100.0f;
}
