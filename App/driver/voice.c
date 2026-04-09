/* Copyright 2025 muzkr https://github.com/muzkr
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#include "driver/voice.h"
#include "driver/systick.h"
#include "py32f071_ll_bus.h"
#include "py32f071_ll_gpio.h"
#include "py32f071_ll_dac.h"
#include "py32f071_ll_tim.h"
#include "py32f071_ll_dma.h"
#include "py32f071_ll_system.h"
#include <string.h>

#define TIMx TIM6
#define DAC_CHANNEL LL_DAC_CHANNEL_1
#define DMA_CHANNEL LL_DMA_CHANNEL_3

#define VOICE_BUF_SIZE (sizeof(uint16_t) * VOICE_BUF_LEN)

static uint16_t DAC_Buf[VOICE_BUF_LEN * 2];

static inline void DMA_Init()
{
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);
    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_SYSCFG);

    LL_SYSCFG_SetDMARemap(DMA1, DMA_CHANNEL, LL_SYSCFG_DMA_MAP_DAC1);

    LL_DMA_InitTypeDef InitStruct;
    //   LL_DMA_StructInit( &InitStruct) ;
    InitStruct.PeriphOrM2MSrcAddress = (uint32_t)DAC_Buf;
    InitStruct.MemoryOrM2MDstAddress = LL_DAC_DMA_GetRegAddr(DAC1, DAC_CHANNEL, LL_DAC_DMA_REG_DATA_12BITS_RIGHT_ALIGNED);
    InitStruct.Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
    InitStruct.Mode = LL_DMA_MODE_CIRCULAR;
    InitStruct.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
    InitStruct.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
    InitStruct.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_HALFWORD;
    InitStruct.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_HALFWORD;
    InitStruct.NbData = sizeof(DAC_Buf) / sizeof(uint16_t);
    InitStruct.Priority = LL_DMA_PRIORITY_HIGH;
    LL_DMA_Init(DMA1, DMA_CHANNEL, &InitStruct);

    LL_DMA_EnableIT_HT(DMA1, DMA_CHANNEL);
    LL_DMA_EnableIT_TC(DMA1, DMA_CHANNEL);
    LL_DMA_EnableChannel(DMA1, DMA_CHANNEL);

    NVIC_SetPriority(DMA1_Channel2_3_IRQn, 3);
    NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
}

static inline void TIM_Init()
{
    // Update freq = 48 MHz / 4 / 1500 == 8 KHz
    LL_TIM_SetPrescaler(TIMx, 3);
    LL_TIM_SetAutoReload(TIMx, 1499);
    LL_TIM_SetTriggerOutput(TIMx, LL_TIM_TRGO_UPDATE);
}

void VOICE_Init()
{
    DMA_Init();

    // Channel 1: PA4
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_4, LL_GPIO_MODE_ANALOG);

    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_DAC1);
    LL_DAC_SetTriggerSource(DAC1, DAC_CHANNEL, LL_DAC_TRIG_EXT_TIM6_TRGO);
    LL_DAC_SetOutputBuffer(DAC1, DAC_CHANNEL, LL_DAC_OUTPUT_BUFFER_ENABLE);
    LL_DAC_EnableDMAReq(DAC1, DAC_CHANNEL);
    LL_DAC_Enable(DAC1, DAC_CHANNEL);

    SYSTICK_DelayUs(15);

    TIM_Init();

    LL_DAC_EnableTrigger(DAC1, DAC_CHANNEL);
}

void VOICE_Start()
{
    LL_DAC_Enable(DAC1, DAC_CHANNEL);
    LL_TIM_DisableCounter(TIMx);

    if (gVoiceBufLen > 0)
    {
        memcpy(DAC_Buf, gVoiceBuf[gVoiceBufReadIndex], VOICE_BUF_SIZE);
        VOICE_BUF_ForwardReadIndex();
        gVoiceBufLen--;
    }
    else
    {
        memset(DAC_Buf, 0, VOICE_BUF_SIZE);
    }
    if (gVoiceBufLen > 0)
    {
        memcpy(DAC_Buf + VOICE_BUF_SIZE,      //
               gVoiceBuf[gVoiceBufReadIndex], //
               VOICE_BUF_SIZE                 //
        );
        VOICE_BUF_ForwardReadIndex();
        gVoiceBufLen--;
    }
    else
    {
        memset(DAC_Buf + VOICE_BUF_SIZE, 0, VOICE_BUF_SIZE);
    }

    LL_DMA_ConfigAddresses(DMA1, DMA_CHANNEL, DAC_Buf,                                                         //
                           LL_DAC_DMA_GetRegAddr(DAC1, DAC_CHANNEL, LL_DAC_DMA_REG_DATA_12BITS_RIGHT_ALIGNED), //
                           LL_DMA_DIRECTION_MEMORY_TO_PERIPH                                                   //
    );
    LL_DMA_EnableChannel(DMA1, DMA_CHANNEL);
    LL_TIM_EnableCounter(TIMx);
}

void VOICE_Stop()
{
    LL_TIM_DisableCounter(TIMx);
    LL_DMA_DisableChannel(DMA1, DMA_CHANNEL);
    LL_DAC_Disable(DAC1, DAC_CHANNEL);
}

void DMA1_Channel2_3_IRQHandler()
{
    if (LL_DMA_IsActiveFlag_HT3(DMA1))
    {
        LL_DMA_ClearFlag_HT3(DMA1);
        if (gVoiceBufLen > 0)
        {
            memcpy(DAC_Buf, gVoiceBuf[gVoiceBufReadIndex], VOICE_BUF_SIZE);
            VOICE_BUF_ForwardReadIndex();
            gVoiceBufLen--;
        }
        else
        {
            memset(DAC_Buf, 0, VOICE_BUF_SIZE);
        }
    }
    if (LL_DMA_IsActiveFlag_TC3(DMA1))
    {
        LL_DMA_ClearFlag_TC3(DMA1);
        if (gVoiceBufLen > 0)
        {
            memcpy(DAC_Buf + VOICE_BUF_SIZE,      //
                   gVoiceBuf[gVoiceBufReadIndex], //
                   VOICE_BUF_SIZE                 //
            );
            VOICE_BUF_ForwardReadIndex();
            gVoiceBufLen--;
        }
        else
        {
            memset(DAC_Buf + VOICE_BUF_SIZE, 0, VOICE_BUF_SIZE);
        }
    }
}
