#include "stm32f4xx_hal.h"

extern ADC_HandleTypeDef hadc1;
extern UART_HandleTypeDef huart2;

// Motor de tiempo de la HAL
void SysTick_Handler(void)
{
  HAL_IncTick();
}

// Interrupción del Potenciómetro (Rojo)
void ADC_IRQHandler(void)
{
  HAL_ADC_IRQHandler(&hadc1);
}

// Interrupción del Encoder (Verde)
void EXTI9_5_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_8);
}

// Interrupción de la UART (Azul)
void USART2_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart2);
}
