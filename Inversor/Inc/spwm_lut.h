/*
 * spwm_lut.h
 *
 *  Created on: Aug 4, 2026
 *      Author: santiago
 */

#ifndef SPWM_LUT_H
#define SPWM_LUT_H

#include <stdint.h>

#define SPWM_SAMPLES     334   // Muestras por ciclo completo de 60 Hz
#define TIMER_PERIOD     1000  // Valor máximo del CCR (ARR + 1)

// Tabla Senoidal modulada en ancho de pulso (0 a 1000)
// Generada con: Duty = (sin(2 * PI * i / 334) * 0.95) * 1000
extern const uint16_t spwm_lut_ch1[SPWM_SAMPLES];
extern const uint16_t spwm_lut_ch2[SPWM_SAMPLES];

#endif /* SPWM_LUT_H */
