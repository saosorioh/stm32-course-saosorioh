/*
 * spwm_config.h
 *
 *  Created on: Aug 4, 2026
 *      Author: santiago
 */

#ifndef SPWM_CONFIG_H
#define SPWM_CONFIG_H

#include "stm32f4xx_hal.h"
#include "spwm_lut.h"

/* Estructuras globales de control */
extern TIM_HandleTypeDef htim1;
extern DMA_HandleTypeDef hdma_tim1_ch1;
extern DMA_HandleTypeDef hdma_tim1_ch2;

/* Funciones de Inicialización */
void SPWM_Init(void);
void SPWM_Start(void);

#endif /* SPWM_CONFIG_H */
