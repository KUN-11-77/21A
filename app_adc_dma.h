#ifndef APP_ADC_DMA_H
#define APP_ADC_DMA_H

#include <stdint.h>
#include <stdbool.h>

#define ADC_SAMPLE_SIZE 2048

void ADC_DMA_Init(void);
void ADC_DMA_Start(void);
void ADC_DMA_Stop(void);
void ADC_DMA_WaitDone(void);
bool ADC_DMA_IsDone(void);

volatile uint16_t *ADC_DMA_GetBuffer(void);
float ADC_DMA_GetSampleRateHz(void);

#endif
