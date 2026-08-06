#include "spwm_config.h"
#include "spwm_lut.h" // Importante para reconocer las tablas y SPWM_SAMPLES

TIM_HandleTypeDef htim1;
DMA_HandleTypeDef hdma_tim1_ch1;
DMA_HandleTypeDef hdma_tim1_ch2;

void SPWM_Init(void)
{
  /* 1. HABILITACIÓN DE RELOJES */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();
  __HAL_RCC_TIM1_CLK_ENABLE();

  /* 2. CONFIGURACIÓN DE PINES GPIO (PA8 -> TIM1_CH1, PA9 -> TIM1_CH2) */
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* 3. CONFIGURACIÓN DEL TIMER 1 */
  TIM_OC_InitTypeDef sConfigOC = {0};

  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 3;                            // PSC = 3 (Divide por 4 -> 21 kHz PWM)
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = TIMER_PERIOD - 1;               // ARR = 999
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  HAL_TIM_PWM_Init(&htim1);

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

  HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1);
  HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2);

  /* 4. CONFIGURACIÓN DEL DMA */

    /* DMA2 Stream 6 - Channel 0 (Para TIM1_CH1) */
    hdma_tim1_ch1.Instance = DMA2_Stream6;
    hdma_tim1_ch1.Init.Channel = DMA_CHANNEL_0;
    hdma_tim1_ch1.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_tim1_ch1.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_tim1_ch1.Init.MemInc = DMA_MINC_ENABLE;
    hdma_tim1_ch1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_tim1_ch1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_tim1_ch1.Init.Mode = DMA_CIRCULAR;
    hdma_tim1_ch1.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_tim1_ch1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdma_tim1_ch1);

    __HAL_LINKDMA(&htim1, hdma[TIM_DMA_ID_CC1], hdma_tim1_ch1);

    /* DMA2 Stream 2 - Channel 6 (Para TIM1_CH2) -> CORRECCIÓN REAL */
    hdma_tim1_ch2.Instance = DMA2_Stream2;
    hdma_tim1_ch2.Init.Channel = DMA_CHANNEL_6;
    hdma_tim1_ch2.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_tim1_ch2.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_tim1_ch2.Init.MemInc = DMA_MINC_ENABLE;
    hdma_tim1_ch2.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_tim1_ch2.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_tim1_ch2.Init.Mode = DMA_CIRCULAR;
    hdma_tim1_ch2.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_tim1_ch2.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdma_tim1_ch2);

    __HAL_LINKDMA(&htim1, hdma[TIM_DMA_ID_CC2], hdma_tim1_ch2);

    /* 5. CONFIGURACIÓN DE INTERRUPCIONES DE DMA */
    HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);

    HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 0, 0); // <--- CORREGIDO Stream 3
    HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);
}
void SPWM_Start(void)
{
  /* Inicia la transferencia DMA circular en ambos canales con sus respectivas tablas */
  HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_1, (uint32_t *)spwm_lut_ch1, SPWM_SAMPLES);
  HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_2, (uint32_t *)spwm_lut_ch2, SPWM_SAMPLES);

  /* Habilita la salida principal del Timer Avanzado (MOE) */
  __HAL_TIM_MOE_ENABLE(&htim1);
}
