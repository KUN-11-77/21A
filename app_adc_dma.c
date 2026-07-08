#include "app_adc_dma.h"
#include "ti_msp_dl_config.h"

/*
 * TIMER_0 is configured for a nominal 40.96 kHz frame rate, but this board's
 * clock measured against a known 1 kHz square wave is about 41.479 kHz.
 */
#define ADC_DMA_SAMPLE_RATE_HZ 41479.1f

static volatile uint16_t gADCSamples[ADC_SAMPLE_SIZE];
static volatile bool gADCDMADone = false;

void ADC_DMA_Init(void)
{
    DL_ADC12_disableConversions(ADC12_0_INST);

    /*
     * TIMER_0 publishes one event per sample period. ADC repeat mode keeps the
     * ADC armed for the full DMA frame while TRIGGER_NEXT preserves timer
     * pacing for each conversion.
     */
    DL_ADC12_initSingleSample(ADC12_0_INST,
        DL_ADC12_REPEAT_MODE_ENABLED,
        DL_ADC12_SAMPLING_SOURCE_AUTO,
        DL_ADC12_TRIG_SRC_EVENT,
        DL_ADC12_SAMP_CONV_RES_12_BIT,
        DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);

    DL_ADC12_configConversionMem(ADC12_0_INST, ADC12_0_ADCMEM_0,
        DL_ADC12_INPUT_CHAN_0,
        DL_ADC12_REFERENCE_VOLTAGE_VDDA,
        DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0,
        DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED,
        DL_ADC12_TRIGGER_MODE_TRIGGER_NEXT,
        DL_ADC12_WINDOWS_COMP_MODE_DISABLED);

    DL_ADC12_setSubscriberChanID(ADC12_0_INST, ADC12_0_INST_SUB_CH);
    /*
     * One timer event produces one ADCMEM0 result and one DMA request. The
     * 2048-sample frame length is controlled by DMA_CH0 transfer size below.
     */
    DL_ADC12_setDMASamplesCnt(ADC12_0_INST, 1U);
    DL_ADC12_enableConversions(ADC12_0_INST);

    DL_TimerA_enableEvent(
        TIMER_0_INST,
        DL_TIMERA_EVENT_ROUTE_1,
        DL_TIMERA_EVENT_ZERO_EVENT);

    DL_TimerA_setPublisherChanID(
        TIMER_0_INST,
        DL_TIMERA_PUBLISHER_INDEX_0,
        ADC12_0_INST_SUB_CH);

    /*
     * DMA copies one 16-bit ADCMEM0 result into gADCSamples per ADC DMA
     * request. The destination increments until one complete 2048-sample frame
     * has been captured.
     */
    DL_DMA_disableChannel(DMA, DMA_CH0_CHAN_ID);

    DL_DMA_setTransferMode(
        DMA,
        DMA_CH0_CHAN_ID,
        DL_DMA_SINGLE_TRANSFER_MODE);

    DL_DMA_setSrcAddr(
        DMA,
        DMA_CH0_CHAN_ID,
        (uint32_t) DL_ADC12_getMemResultAddress(
            ADC12_0_INST,
            DL_ADC12_MEM_IDX_0));

    DL_DMA_setDestAddr(
        DMA,
        DMA_CH0_CHAN_ID,
        (uint32_t) &gADCSamples[0]);

    DL_DMA_setTransferSize(
        DMA,
        DMA_CH0_CHAN_ID,
        ADC_SAMPLE_SIZE);

    DL_ADC12_clearInterruptStatus(
        ADC12_0_INST,
        DL_ADC12_INTERRUPT_DMA_DONE);

    DL_ADC12_enableInterrupt(
        ADC12_0_INST,
        DL_ADC12_INTERRUPT_DMA_DONE);

    NVIC_EnableIRQ(ADC12_0_INST_INT_IRQN);
}

void ADC_DMA_Start(void)
{
    gADCDMADone = false;

    /* Reload DMA state so each capture starts at the beginning of the frame. */
    DL_DMA_disableChannel(DMA, DMA_CH0_CHAN_ID);

    DL_DMA_setDestAddr(
        DMA,
        DMA_CH0_CHAN_ID,
        (uint32_t) &gADCSamples[0]);

    DL_DMA_setTransferSize(
        DMA,
        DMA_CH0_CHAN_ID,
        ADC_SAMPLE_SIZE);

    DL_DMA_enableChannel(DMA, DMA_CH0_CHAN_ID);

    DL_ADC12_clearInterruptStatus(
        ADC12_0_INST,
        DL_ADC12_INTERRUPT_DMA_DONE);

    DL_ADC12_enableDMA(ADC12_0_INST);
    DL_TimerA_startCounter(TIMER_0_INST);
}

void ADC_DMA_Stop(void)
{
    DL_TimerA_stopCounter(TIMER_0_INST);
    DL_DMA_disableChannel(DMA, DMA_CH0_CHAN_ID);
    DL_ADC12_disableDMA(ADC12_0_INST);
}

void ADC_DMA_WaitDone(void)
{
    while (gADCDMADone == false) {
        __WFI();
    }
}

bool ADC_DMA_IsDone(void)
{
    return gADCDMADone;
}

volatile uint16_t *ADC_DMA_GetBuffer(void)
{
    return gADCSamples;
}

void ADC12_0_INST_IRQHandler(void)
{
    switch (DL_ADC12_getPendingInterrupt(ADC12_0_INST)) {
        case DL_ADC12_IIDX_DMA_DONE:
            ADC_DMA_Stop();
            gADCDMADone = true;
            break;

        default:
            break;
    }
}

float ADC_DMA_GetSampleRateHz(void)
{
    return ADC_DMA_SAMPLE_RATE_HZ;
}
