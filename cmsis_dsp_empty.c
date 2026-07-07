/*
 * Copyright (c) 2021, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "arm_math.h"
#include "ti_msp_dl_config.h"
#include <stdint.h>
#include <stdbool.h>

#define ADC_SAMPLE_SIZE 1024

volatile uint16_t gADCSamples[ADC_SAMPLE_SIZE];
volatile bool gADCDMADone = false;

int main(void)
{
    SYSCFG_DL_init();

    /*
     * DMA 源地址：ADC MEMRES0
     */
    DL_DMA_setSrcAddr(
        DMA,
        DMA_CH0_CHAN_ID,
        (uint32_t) DL_ADC12_getMemResultAddress(ADC12_0_INST, DL_ADC12_MEM_IDX_0)
    );

    /*
     * DMA 目的地址：采样 buffer
     */
    DL_DMA_setDestAddr(
        DMA,
        DMA_CH0_CHAN_ID,
        (uint32_t) &gADCSamples[0]
    );

    /*
     * DMA 传输长度：1024 点
     */
    DL_DMA_setTransferSize(
        DMA,
        DMA_CH0_CHAN_ID,
        ADC_SAMPLE_SIZE
    );

    /*
     * 使能 DMA channel
     */
    DL_DMA_enableChannel(DMA, DMA_CH0_CHAN_ID);

    /*
     * 开 ADC 中断，用来接收 DMA_DONE
     */
    NVIC_EnableIRQ(ADC12_0_INST_INT_IRQN);

    gADCDMADone = false;

    /*
     * 启动 ADC
     * 如果你是软件触发 ADC，用这一句。
     */
    DL_ADC12_startConversion(ADC12_0_INST);

    /*
     * 如果你是 TIMA0 触发 ADC，则上面那句可能不用，
     * 改成启动定时器：
     *
     * DL_TimerA_startCounter(TIMER_0_INST);
     */

    while (gADCDMADone == false) {
        __WFE();
    }

    /*
     * 程序运行到这里，gADCSamples 里应该已经有 1024 个 ADC 数据。
     * 可以打断点观察数组。
     */
    __BKPT(0);

    while (1) {
        __WFI();
    }
}

void ADC12_0_INST_IRQHandler(void)
{
    switch (DL_ADC12_getPendingInterrupt(ADC12_0_INST)) {
        case DL_ADC12_IIDX_DMA_DONE:
            gADCDMADone = true;
            break;

        default:
            break;
    }
}
