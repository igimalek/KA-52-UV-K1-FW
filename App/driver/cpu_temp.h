#ifndef DRIVER_CPU_TEMP_H
#define DRIVER_CPU_TEMP_H

#include <stdint.h>

/**
 * @brief Read the internal CPU temperature sensor.
 * @return Temperature in deci-Celsius (e.g. 255 = 25.5 °C).
 *         Uses factory calibration points at 30 °C and 105 °C stored in
 *         the PY32F071 OTP area.  Falls back to a typical-curve estimate
 *         when the OTP values are blank (0xFFFF / 0x0000).
 */
int16_t CpuTemp_ReadDeciCelsius(void);

/**
 * @brief Read the three 32-bit words of the PY32F071 unique device ID.
 * @param w0  Output: UID word 0 (bits [31:0]).
 * @param w1  Output: UID word 1 (bits [63:32]).
 * @param w2  Output: UID word 2 (bits [95:64]).
 */
void CpuInfo_GetUID(uint32_t *w0, uint32_t *w1, uint32_t *w2);

#endif /* DRIVER_CPU_TEMP_H */
