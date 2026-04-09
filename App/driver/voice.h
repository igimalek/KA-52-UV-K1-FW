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

#ifndef DRIVER_VOICE_H
#define DRIVER_VOICE_H

#include <stdint.h>

#define VOICE_BUF_CAP 4
#define VOICE_BUF_LEN 160

extern uint16_t gVoiceBuf[VOICE_BUF_CAP][VOICE_BUF_LEN];
extern uint8_t gVoiceBufReadIndex;
extern uint8_t gVoiceBufWriteIndex;
extern uint8_t gVoiceBufLen;

static inline void VOICE_BUF_ForwardReadIndex()
{
    gVoiceBufReadIndex = (1 + gVoiceBufReadIndex) % VOICE_BUF_CAP;
}

static inline void VOICE_BUF_ForwardWriteIndex()
{
    gVoiceBufWriteIndex = (1 + gVoiceBufWriteIndex) % VOICE_BUF_CAP;
}

void VOICE_Init();
void VOICE_Start();
void VOICE_Stop();

#endif // DRIVER_VOICE_H
