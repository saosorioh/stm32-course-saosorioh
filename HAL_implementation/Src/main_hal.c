/*
 * main_hal.c
 *
 *  Created on: Jun 11, 2026
 *      Author: santiago
 */

#include "stm32f4xx_hal.h"

/* TIM3 handle — must be global so stm32f4xx_it.c can access it */
TIM_HandleTypeDef htim3;

USART_HandleTypeDef husart2;

/* Private function prototypes */
static void SystemClock_Config(void);
static void gpio_Init(void);
static void tim3_Init(void);
static void usart2_Init(void);



int main(void)
{
    HAL_Init();           /* initialize HAL: SysTick, cache, priority grouping */
    SystemClock_Config(); /* configure clock tree: HSI at 16 MHz               */
    gpio_Init();          /* configure PA5 as push-pull output                  */
    tim3_Init();          /* configure TIM3: update event every 250 ms          */

    while (1)
    {
        /* application loop — LED toggling happens in the callback */
    }
}

/*
 * SystemClock_Config
 * Uses HSI internal oscillator at 16 MHz
 * No PLL — simplest possible clock configuration
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* HSI is already on at reset — confirm and use it */
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    /* Select HSI as SYSCLK — all bus dividers set to 1 */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_SYSCLK |
                                       RCC_CLOCKTYPE_HCLK   |
                                       RCC_CLOCKTYPE_PCLK1  |
                                       RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK  = 16 MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;     /* APB1  = 16 MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;     /* APB2  = 16 MHz */

    /* FLASH_LATENCY_0 = zero wait states, correct for 16 MHz */
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
}

/*
 * gpio_Init
 * Configures PA5 as push-pull output — onboard LED on Nucleo board
 */
static void gpio_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable GPIOA clock on AHB1 bus
       Same as bare-metal: RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN */
    __HAL_RCC_GPIOH_CLK_ENABLE();

    /* Configure PA5 */
    GPIO_InitStruct.Pin   = GPIO_PIN_1;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    //Cargando la configuracion en los FSR del MCU//
    HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);
}

/*
 * tim3_Init
 * Configures TIM3 to generate an update event every 250 ms
 *
 * Clock chain:
 *   HSI (16 MHz) → APB1 (16 MHz) → TIM3 clock (16 MHz)
 *
 * PSC = 15999  →  tick = 16,000,000 / (15999 + 1) = 1,000 Hz  (1 ms per tick)
 * ARR = 249    →  period = (249 + 1) x 1 ms = 250 ms
 */
static void tim3_Init(void)
{
    /* Enable TIM3 clock on APB1 bus */
    __HAL_RCC_TIM3_CLK_ENABLE();

    /* Configure TIM3 base */
    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 15999;
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = 249;
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    // Cargamos la configuracion en los registros FSR del MCU) //
    HAL_TIM_Base_Init(&htim3);

    /* Start TIM3 in interrupt mode — enables the update event interrupt */
    HAL_TIM_Base_Start_IT(&htim3);

    /* Enable TIM3 interrupt line in the NVIC */
    HAL_NVIC_EnableIRQ(TIM3_IRQn);
}

static void usart2_Init(void){


	    GPIO_InitTypeDef GPIO_InitTx = {0};
	    /* Enable GPIOA clock on AHB1 bus
	       Same as bare-metal: RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN */
	    __HAL_RCC_GPIOA_CLK_ENABLE();

	    /* Configure PA5 */
	    GPIO_InitTx.Pin   = GPIO_PIN_2;
	    GPIO_InitTx.Mode  = GPIO_MODE_AF_PP;
	    GPIO_InitTx.Pull  = GPIO_NOPULL;
	    GPIO_InitTx.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	    GPIO_InitTx.Alternate = GPIO_AF7_USART2;


	    //Cargando la configuracion en los FSR del MCU//
	    HAL_GPIO_Init(GPIOA, &GPIO_InitTx);





	__HAL_RCC_USART1_CLK_ENABLE();

	 //Configuracion 19200 8N1  -  8 bit data, anly tx //
	husart2.Instance = USART2;
	husart2.Init.BaudRate = 19200;
	husart2.Init.Mode = USART_MODE_TX;
	husart2.Init.Parity = USART_PARITY_NONE;
	husart2.Init.StopBits = USART_STOPBITS_1;
	husart2.Init.WordLength = USART_WORDLENGTH_8B;

	HAL_USART_Init(&husart2);
	__NOP();

	HAL_USART_Transmit(&husart2, "Hola Mundo", 11, 100);
	__NOP();

}




/*
 * HAL_TIM_PeriodElapsedCallback
 * Called automatically by HAL_TIM_IRQHandler() every time a timer
 * update event fires. Shared by all timers — always check htim->Instance.
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)
    {
        HAL_GPIO_TogglePin(GPIOH, GPIO_PIN_1);
    }
}
