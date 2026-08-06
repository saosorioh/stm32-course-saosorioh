#include "main.h"

extern UART_HandleTypeDef huart2;

// Servidor de interrupción de la UART2
void USART2_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart2);
}

// Callback al recibir 1 carácter: Cero trabajo pesado aquí
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        g_events.flag_uart_rx = 1; // Solo se activa la bandera volatile
    }
}
