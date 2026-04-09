#include "cpu_temp.h"
#include "py32f071_ll_adc.h"
#include "py32f071_ll_utils.h"

/* Factory temperature calibration OTP addresses (PY32F071) */
#define TS_CAL1_ADDR  ((volatile uint16_t *)0x1FFF3228U)  /* raw ADC at 30 °C  */
#define TS_CAL2_ADDR  ((volatile uint16_t *)0x1FFF3230U)  /* raw ADC at 105 °C */
#define VREFINT_MV    1200U   /* typical internal reference voltage (mV) */
#define VDDA_NOMINAL_MV  3300U /* nominal VDDA used for calibration data  */

/* Iterations to poll for end-of-conversion (~2 ms at 48 MHz with slow sampling) */
#define ADC_CONVERSION_TIMEOUT_ITERATIONS  20000

/* Perform a single ADC conversion on the given internal channel.
 * Saves and restores the regular-channel sequencer so battery readings
 * are unaffected after the call. */
static uint16_t ReadInternalChannel(uint32_t channel, uint32_t path)
{
    /* Enable the internal path (TEMPSENSOR or VREFINT) */
    LL_ADC_SetCommonPathInternalCh(ADC1_COMMON, path);

    /* Configure rank-1 to the requested internal channel */
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, channel);
    LL_ADC_SetChannelSamplingTime(ADC1, channel,
                                  LL_ADC_SAMPLINGTIME_239CYCLES_5);

    /* Trigger and wait */
    LL_ADC_REG_StartConversionSWStart(ADC1);
    for (int i = 0; i < ADC_CONVERSION_TIMEOUT_ITERATIONS; i++) {
        if (LL_ADC_IsActiveFlag_EOS(ADC1))
            break;
    }
    uint16_t result = LL_ADC_REG_ReadConversionData12(ADC1);
    LL_ADC_ClearFlag_EOS(ADC1);

    /* Restore: disable internal path, put battery channel (CH8) back */
    LL_ADC_SetCommonPathInternalCh(ADC1_COMMON, LL_ADC_PATH_INTERNAL_NONE);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1,
                                  LL_ADC_CHANNEL_8);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_8,
                                  LL_ADC_SAMPLINGTIME_41CYCLES_5);

    return result;
}

static uint16_t GetVref_mV(void)
{
    uint16_t raw = ReadInternalChannel(LL_ADC_CHANNEL_VREFINT,
                                       LL_ADC_PATH_INTERNAL_VREFINT);
    if (raw == 0u)
        return 3300u;
    return (uint16_t)(((uint32_t)VREFINT_MV * 4095u) / (uint32_t)raw);
}

int16_t CpuTemp_ReadDeciCelsius(void)
{
    uint16_t ts_cal1 = *TS_CAL1_ADDR;
    uint16_t ts_cal2 = *TS_CAL2_ADDR;
    uint16_t ts_data = ReadInternalChannel(LL_ADC_CHANNEL_TEMPSENSOR,
                                           LL_ADC_PATH_INTERNAL_TEMPSENSOR);
    uint16_t vdda_mv = GetVref_mV();

    /* Normalise reading to the calibration baseline of VDDA_NOMINAL_MV */
    uint32_t ts_norm = ((uint32_t)ts_data * vdda_mv) / VDDA_NOMINAL_MV;

    /* Sanity-check OTP calibration values */
    if (ts_cal2 <= ts_cal1 || ts_cal1 == 0xFFFFu || ts_cal1 == 0u) {
        /* Fallback: typical 0.75 V at 30 °C, ~2.5 mV/°C slope */
        uint32_t mv = (ts_norm * VDDA_NOMINAL_MV) / 4095u;
        return (int16_t)(300 + ((int32_t)mv - 750) * 4);
    }

    /* Linear interpolation between 30 °C (cal1) and 105 °C (cal2).
     * Result in deci-Celsius: 300 + delta * 750 / range              */
    return (int16_t)(300 +
        (int32_t)(((int32_t)ts_norm - (int32_t)ts_cal1) * 750 /
                  (int32_t)(ts_cal2 - ts_cal1)));
}

void CpuInfo_GetUID(uint32_t *w0, uint32_t *w1, uint32_t *w2)
{
    *w0 = LL_GetUID_Word0();
    *w1 = LL_GetUID_Word1();
    *w2 = LL_GetUID_Word2();
}
