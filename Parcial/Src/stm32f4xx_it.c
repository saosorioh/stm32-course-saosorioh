#include "stm32f4xx_it.h"
#include "main.h"

/* Handles externos declarados en main.c */
extern UART_HandleTypeDef huart2;
extern TIM_HandleTypeDef htim2;

/**
  * @brief Interrupción de SysTick (Reloj de la HAL)
  */
void SysTick_Handler(void) {
    HAL_IncTick();
}

/**
  * @brief Interrupción del TIM2 (Dispara Callback para el LED en PH1)
  */
void TIM2_IRQHandler(void) {
   HAL_TIM_IRQHandler(&htim2);
}

/*
 * Servidor de interrupción de la USART2 (Lo usaremos en el Punto 3)
 */
void USART2_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart2);
}
