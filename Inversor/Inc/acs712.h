/*
 * acs712.h
 *
 *  Created on: Aug 5, 2026
 *      Author: santiago
 */

#ifndef ACS712_H
#define ACS712_H

#include "stm32f4xx_hal.h"

/* Muestras por ciclo aprox: 3000Hz / 60Hz = 50. Con 150 muestras -> ~3 ciclos
 * completos en el buffer circular, suficiente para graficar en OLED/UART */
#define ACS712_SAMPLES   150

/* Sensibilidad del ACS712 de 5A: 185 mV/A (según datasheet) */
#define ACS712_MV_PER_A       185
#define ADC_VREF_MV           3300
#define ADC_MAX_COUNTS        4095

/* Divisor resistivo de proteccion en la entrada PA0 (Opcion A):
 * ACS712_OUT --[R1=1k]--+--[R2=2.7k]-- GND
 *                       |
 *                     PA0 (ADC1_IN0)
 * Atenuacion = R2/(R1+R2) = 2.7/3.7 = 0.7297
 * Con esto, 4.5V (maximo teorico de salida del sensor) -> 3.28V en PA0 (seguro) */
#define ACS712_DIVIDER_RATIO  0.7297f

/* Offset de cero-corriente del ACS712 ANTES del divisor (Vcc_sensor/2 = 5V/2) */
#define ACS712_ZERO_OFFSET_MV 2500.0f

extern ADC_HandleTypeDef  hadc1;
extern DMA_HandleTypeDef  hdma_adc1;
extern TIM_HandleTypeDef  htim3;

/* Buffer circular: la DMA lo sobre-escribe continuamente, sin intervención del CPU */
extern volatile uint16_t acs712_buffer[ACS712_SAMPLES];

void ACS712_Init(void);
float ACS712_RawToAmps(uint16_t raw_adc);

#endif /* ACS712_H */
