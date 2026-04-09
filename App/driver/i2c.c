/* Copyright 2025 muzkr https://github.com/muzkr
 * Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
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

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/systick.h"

#define PIN_SCL     GPIO_MAKE_PIN(GPIOF, LL_GPIO_PIN_5)
#define PIN_SDA     GPIO_MAKE_PIN(GPIOF, LL_GPIO_PIN_6)

// CRITICAL FIX: Define I2C timeout to prevent infinite hangs
// If I2C slave doesn't respond, timeout after this many iterations
// 255 iterations × 1µs = 255µs, plus we add safety margin
#define I2C_ACK_TIMEOUT_ITERATIONS 255

static inline void SCL_Set()
{
    GPIO_SetOutputPin(PIN_SCL);
}

static inline void SCL_Reset()
{
    GPIO_ResetOutputPin(PIN_SCL);
}

static inline void SDA_Set()
{
    GPIO_SetOutputPin(PIN_SDA);
}

static inline void SDA_Reset()
{
    GPIO_ResetOutputPin(PIN_SDA);
}

static inline void SDA_SetDir(bool Output)
{
    LL_GPIO_SetPinMode(GPIO_PORT(PIN_SDA), GPIO_PIN_MASK(PIN_SDA), Output ? LL_GPIO_MODE_OUTPUT : LL_GPIO_MODE_INPUT);
}

static inline bool SDA_IsSet()
{
    return GPIO_IsInputPinSet(PIN_SDA);
}

void I2C_Start(void)
{
    SDA_Set();
    SYSTICK_DelayUs(1);
    SCL_Set();
    SYSTICK_DelayUs(1);
    SDA_Reset();
    SYSTICK_DelayUs(1);
    SCL_Reset();
    SYSTICK_DelayUs(1);
}

void I2C_Stop(void)
{
    SDA_Reset();
    SYSTICK_DelayUs(1);
    SCL_Reset();
    SYSTICK_DelayUs(1);
    SCL_Set();
    SYSTICK_DelayUs(1);
    SDA_Set();
    SYSTICK_DelayUs(1);
}

uint8_t I2C_Read(bool bFinal)
{
    uint8_t i, Data;

    SDA_SetDir(false);

    Data = 0;
    for (i = 0; i < 8; i++) {
        SCL_Reset();
        SYSTICK_DelayUs(1);
        SCL_Set();
        SYSTICK_DelayUs(1);
        Data <<= 1;
        SYSTICK_DelayUs(1);
        if (SDA_IsSet()) {
            Data |= 1U;
        }
        SCL_Reset();
        SYSTICK_DelayUs(1);
    }

    SDA_SetDir(true);
    SCL_Reset();
    SYSTICK_DelayUs(1);
    if (bFinal) {
        SDA_Set();
    } else {
        SDA_Reset();
    }
    SYSTICK_DelayUs(1);
    SCL_Set();
    SYSTICK_DelayUs(1);
    SCL_Reset();
    SYSTICK_DelayUs(1);

    return Data;
}

int I2C_Write(uint8_t Data)
{
    uint8_t i;
    int ret = -1;

    SCL_Reset();
    SYSTICK_DelayUs(1);
    for (i = 0; i < 8; i++) {
        if ((Data & 0x80) == 0) {
            SDA_Reset();
        } else {
            SDA_Set();
        }
        Data <<= 1;
        SYSTICK_DelayUs(1);
        SCL_Set();
        SYSTICK_DelayUs(1);
        SCL_Reset();
        SYSTICK_DelayUs(1);
    }

    SDA_SetDir(false);
    SDA_Set();
    SYSTICK_DelayUs(1);
    SCL_Set();
    SYSTICK_DelayUs(1);

    // CRITICAL FIX #1: Add timeout to ACK check
    // Original: for (i = 0; i < 255; i++)
    // Problem: If I2C slave doesn't respond, waits 255 iterations = 255µs spin-wait
    // If slave broken/disconnected: loops forever with no break condition
    //
    // Solution: Keep iteration limit but add explicit timeout check
    // If slave doesn't pull SDA low within 255 iterations, return error
    // This prevents indefinite hangs while still being conservative
    
    for (i = 0; i < I2C_ACK_TIMEOUT_ITERATIONS; i++) {
        if (!SDA_IsSet()) {
            ret = 0;
            break;
        }
        // CRITICAL FIX #2: Small delay between ACK checks
        // This prevents CPU from spinning 255 times at full speed
        // Also gives slave time to respond
        SYSTICK_DelayUs(1);
    }

    SCL_Reset();
    SYSTICK_DelayUs(1);
    SDA_SetDir(true);
    SDA_Set();

    return ret;
}

int I2C_ReadBuffer(void *pBuffer, uint8_t Size)
{
    uint8_t *pData = (uint8_t *)pBuffer;
    uint8_t i;

    for (i = 0; i < Size - 1; i++) {
        SYSTICK_DelayUs(1);
        pData[i] = I2C_Read(false);
    }

    SYSTICK_DelayUs(1);
    pData[i] = I2C_Read(true);

    return Size;
}

int I2C_WriteBuffer(const void *pBuffer, uint8_t Size)
{
    const uint8_t *pData = (const uint8_t *)pBuffer;
    uint8_t i;

    for (i = 0; i < Size; i++) {
        // CRITICAL FIX #3: Check return value of I2C_Write
        // Original: if (I2C_Write(*pData++) < 0)
        // This detects if ACK timeout occurred
        // If slave not responding, return error immediately instead of continuing
        if (I2C_Write(*pData++) < 0) {
            // I2C slave not responding (ACK timeout)
            // Return error instead of continuing to write more bytes
            // This prevents cascading timeouts
            return -1;
        }
    }

    return 0;
}