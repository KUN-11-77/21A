#ifndef APP_FFT_H
#define APP_FFT_H

#include <stdbool.h>
#include <stdint.h>
#include "app_adc_dma.h"

#define APP_FFT_SIZE ADC_SAMPLE_SIZE
#define APP_ADC_FULL_SCALE 4095.0f
#define APP_ADC_REF_VOLTAGE 3.3f

#ifndef FFT_USE_AUTO_BASE_BIN
#define FFT_USE_AUTO_BASE_BIN 1U
#endif

#ifndef FFT_BASE_FREQ_HZ
#define FFT_BASE_FREQ_HZ 1000.0f
#endif

#ifndef FFT_HARMONIC_MAX
#define FFT_HARMONIC_MAX 10U
#endif

#ifndef FFT_SEARCH_HALF_WIDTH
#define FFT_SEARCH_HALF_WIDTH 3U
#endif

#ifndef FFT_MIN_BASE_FREQ_HZ
#define FFT_MIN_BASE_FREQ_HZ 100.0f
#endif

#ifndef FFT_MAX_BASE_FREQ_HZ
#define FFT_MAX_BASE_FREQ_HZ 10000.0f
#endif

void FFT_Process(volatile uint16_t *adcBuffer);

const float *FFT_GetMagnitudeBuffer(void);

uint32_t FFT_GetPeakBin(void);
float FFT_GetPeakFrequencyHz(void);
float FFT_GetPeakAmplitudeAdc(void);
float FFT_GetPeakAmplitudeVolt(void);

uint32_t FFT_GetBaseBin(void);
float FFT_GetBaseFrequencyHz(void);
float FFT_GetTimeDomainFrequencyHz(void);

float FFT_GetBinFrequencyHz(uint32_t bin);
float FFT_GetBinAmplitudeAdc(uint32_t bin);
float FFT_GetBinAmplitudeVolt(uint32_t bin);

float FFT_GetTHDPercent(void);
bool FFT_IsFundamentalValid(void);
float FFT_GetHarmonicAmplitudeVolt(uint32_t harmonic);
float FFT_GetHarmonicAmplitudeAdc(uint32_t harmonic);
uint32_t FFT_GetHarmonicBin(uint32_t harmonic);
uint32_t FFT_GetExpectedHarmonicBin(uint32_t harmonic);
uint32_t FFT_GetHarmonicSearchHalfWidth(void);

float FFT_AdcAmplitudeToVolt(float adcAmplitude);

#endif
