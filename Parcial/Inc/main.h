#ifndef MAIN_H
#define MAIN_H

#include "stm32f4xx_hal.h"
#include "lcd_i2c.h"
#include "mpu6050.h"

// Banderas de eventos asíncronos en RAM
typedef struct {
    volatile uint8_t flag_uart_rx;
} EventFlags_t;

extern EventFlags_t g_events;
extern volatile uint8_t g_rx_cmd;

#endif /* MAIN_H */
