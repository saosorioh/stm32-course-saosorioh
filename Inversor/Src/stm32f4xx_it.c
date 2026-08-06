#include "stm32f4xx_it.h"
#include "main.h"

/* Handles externos declarados en main.c / spwm_config.c / acs712.c */
extern UART_HandleTypeDef huart2;
extern TIM_HandleTypeDef htim2;
extern DMA_HandleTypeDef hdma_tim1_ch1;
extern DMA_HandleTypeDef hdma_tim1_ch2;
extern DMA_HandleTypeDef hdma_adc1;

/**
  * @brief Interrupción de SysTick (Reloj de la HAL)
  */
void SysTick_Handler(void) {
    HAL_IncTick();
}

/**
  * @brief Interrupción para DMA2 Stream 6 (TIM1_CH1)
  */
void DMA2_Stream6_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_tim1_ch1);
}

/**
  * @brief Interrupción para DMA2 Stream 2 (TIM1_CH2) -- corregido de Stream3 a Stream2
  */
void DMA2_Stream2_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_tim1_ch2);
}

/**
  * @brief Interrupción para DMA2 Stream 0 (ADC1 - ACS712 en PA0)
  */
void DMA2_Stream0_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_adc1);
}

/**
  * @brief Interrupción de recepción USART2 -- delega en HAL, que llama a
  * HAL_UART_RxCpltCallback (definido en main.c)
  */
void USART2_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart2);
}
