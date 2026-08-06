#include "stm32f4xx_hal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// ===================================================================
// 1. Estructuras y Variables Globales
// ===================================================================
TIM_HandleTypeDef htim3;
DMA_HandleTypeDef hdma_tim3_up; // Global para el DMA
UART_HandleTypeDef huart2;      // Para CoolTerm

// Tabla ampliada a 1000 pasos para garantizar fluidez a alta frecuencia PWM
#define TABLA_TAMANO 1000
static uint32_t tabla_brillo_azul[TABLA_TAMANO];

char buffer_uart[100];
uint32_t contador_heartbeat = 0;

// ===================================================================
// 2. Prototipos
// ===================================================================
void SystemClock_Config(void);
void Crear_Tabla_Brillo(void);
void Inicializar_Pin_Estado(void);
void Inicializar_TIM3_PWM_Azul(void);
void Inicializar_DMA1_TIM3(void);
void Inicializar_UART2(void);
void Mi_UART_Transmit(char *texto);

// ===================================================================
// 3. Main
// ===================================================================
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  Crear_Tabla_Brillo();
  Inicializar_Pin_Estado();
  Inicializar_TIM3_PWM_Azul();
  Inicializar_DMA1_TIM3();
  Inicializar_UART2();

  __HAL_LINKDMA(&htim3, hdma[TIM_DMA_ID_UPDATE], hdma_tim3_up);

  HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);

  Mi_UART_Transmit("\r\n============================================\r\n");
  Mi_UART_Transmit("   PWM ALTA FRECUENCIA (1000Hz) + DMA OK\r\n");
  Mi_UART_Transmit("============================================\r\n");

  // Secuencia manual de arranque DMA
  HAL_DMA_Start_IT(htim3.hdma[TIM_DMA_ID_UPDATE],
                   (uint32_t)tabla_brillo_azul,
                   (uint32_t)&htim3.Instance->CCR3,
                   TABLA_TAMANO);

  __HAL_TIM_ENABLE_DMA(&htim3, TIM_DMA_UPDATE);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);

  while (1)
  {
    // === 1. PARPADEO PIN PH1 ===
    contador_heartbeat++;
    if (contador_heartbeat >= 10)
    {
      HAL_GPIO_TogglePin(GPIOH, GPIO_PIN_1);
      contador_heartbeat = 0;
    }

    // === 2. TELEMETRÍA EN COOLTERM ===
    uint32_t ccr_actual = TIM3->CCR3;
    uint32_t porcentaje = (ccr_actual * 100) / 1000;

    sprintf(buffer_uart, "[DMA ULTRA-SUAVE] -> CCR3: %4lu | Intensidad Azul: %3lu%%\r\n",
            ccr_actual, porcentaje);

    Mi_UART_Transmit(buffer_uart);

    HAL_Delay(50);
  }
}

// ===================================================================
// 4. Funciones Auxiliares
// ===================================================================
void Mi_UART_Transmit(char *texto)
{
  HAL_UART_Transmit(&huart2, (uint8_t*)texto, strlen(texto), 100);
}

void Crear_Tabla_Brillo(void)
{
  for (uint32_t i = 0; i < TABLA_TAMANO; i++)
  {
    // Curva senoidal con resolución de 1000 puntos
    float angulo = ((float)i / (float)(TABLA_TAMANO - 1)) * 3.14159f;
    tabla_brillo_azul[i] = (uint32_t)(sinf(angulo) * 1000.0f);
  }
}

void Inicializar_Pin_Estado(void)
{
  __HAL_RCC_GPIOH_CLK_ENABLE();
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);
}

void Inicializar_TIM3_PWM_Azul(void)
{
  __HAL_RCC_TIM3_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  htim3.Instance = TIM3;
  // Prescaler = 83 -> Reloj PWM a 1000 Hz (Inaudible e invisible para el ojo humano)
  htim3.Init.Prescaler = 83;
  htim3.Init.Period = 1000;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  HAL_TIM_PWM_Init(&htim3);

  TIM_OC_InitTypeDef sConfigOC = {0};
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

  HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3);
}

void Inicializar_DMA1_TIM3(void)
{
  __HAL_RCC_DMA1_CLK_ENABLE();

  hdma_tim3_up.Instance                 = DMA1_Stream2;
  hdma_tim3_up.Init.Channel             = DMA_CHANNEL_5;
  hdma_tim3_up.Init.Direction           = DMA_MEMORY_TO_PERIPH;
  hdma_tim3_up.Init.PeriphInc           = DMA_PINC_DISABLE;
  hdma_tim3_up.Init.MemInc              = DMA_MINC_ENABLE;
  hdma_tim3_up.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  hdma_tim3_up.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
  hdma_tim3_up.Init.Mode                = DMA_CIRCULAR;
  hdma_tim3_up.Init.Priority            = DMA_PRIORITY_MEDIUM;
  hdma_tim3_up.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;

  HAL_DMA_Init(&hdma_tim3_up);
}

void Inicializar_UART2(void)
{
  __HAL_RCC_USART2_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  HAL_UART_Init(&huart2);
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = 16;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}
