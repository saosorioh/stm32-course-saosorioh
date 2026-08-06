#ifndef MAIN_H
#define MAIN_H

#include "stm32f4xx_hal.h"

// Banderas de eventos asíncronos en RAM
typedef struct {
    volatile uint8_t flag_uart_rx;
} EventFlags_t;

extern EventFlags_t g_events;
extern volatile uint8_t g_rx_cmd;

// Direcciones I2C (Shift de 1 bit a la izquierda para la HAL)
#define LCD_ADDR       (0x27 << 1) // 0x4E (O cambiar a 0x3F << 1 según el módulo)
#define MPU6050_ADDR   (0x69 << 1) // 0xD2 (Con AD0 conectado a 3.3V)

#endif
