#ifndef TELEMETRIA_H
#define TELEMETRIA_H

#include "stm32f4xx_hal.h"
#include "spwm_lut.h"

extern UART_HandleTypeDef huart2;

void UART_Init(void);
void Telemetria_ImprimirTablaTexto(void);
void Telemetria_GraficaASCII(void);
void Telemetria_GraficaCorrienteACS712(const uint16_t *buffer, uint16_t len);

#endif /* TELEMETRIA_H */
