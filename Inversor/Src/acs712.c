/*
 * acs712.c
 *
 *  Created on: Aug 5, 2026
 *      Author: santiago
 */

#include "acs712.h"

ADC_HandleTypeDef  hadc1;
DMA_HandleTypeDef  hdma_adc1;
TIM_HandleTypeDef  htim3;

volatile uint16_t acs712_buffer[ACS712_SAMPLES] = {0};

/* -------------------------------------------------------------------------------
 * TIM3: SOLO genera el evento de trigger (TRGO) para el ADC, sin PWM ni pines.
 * f_TIM3 = 96MHz (APB1 a 48MHz x2, misma logica que TIM1 en APB2 sin division)
 * PSC=31, ARR=999 -> 96,000,000 / (32 * 1000) = 3000 Hz exactos
 * ------------------------------------------------------------------------------- */
static void TIM3_Trigger_Init(void)
{
    __HAL_RCC_TIM3_CLK_ENABLE();

    TIM_MasterConfigTypeDef sMasterConfig = {0};

    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 31;    /* PSC+1 = 32 */
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = 999;   /* ARR+1 = 1000 */
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_Base_Init(&htim3);

    /* Selecciona el evento de actualizacion (Update) como salida TRGO hacia el ADC */
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig);
}

/* -------------------------------------------------------------------------------
 * ADC1 en PA0 (ACS712_OUT via divisor 1k/2.7k), disparado por TIM3_TRGO,
 * con DMA2 Stream0/Channel0 en modo circular hacia acs712_buffer[]
 * ------------------------------------------------------------------------------- */
static void ADC1_DMA_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    ADC_ChannelConfTypeDef sConfig = {0};

    /* --- Pin PA0 en modo analogo --- */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin  = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* --- ADC1 --- */
    __HAL_RCC_ADC1_CLK_ENABLE();

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4; /* APB2=96MHz/4 = 24MHz (<36MHz max) */
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode          = DISABLE;                  /* un solo canal */
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    hadc1.Init.ContinuousConvMode    = DISABLE;                  /* una conversion por trigger de TIM3 */
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv      = ADC_EXTERNALTRIGCONV_T3_TRGO;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc1.Init.DMAContinuousRequests = ENABLE;                   /* la DMA sigue pidiendo tras cada trigger */

    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        while (1) { __NOP(); }
    }

    sConfig.Channel      = ADC_CHANNEL_0;   /* PA0 */
    sConfig.Rank         = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES; /* (15+12)/24MHz = 1.1us, sobra margen frente a 333us del periodo de trigger */
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        while (1) { __NOP(); }
    }

    /* --- DMA2 Stream0 / Channel0 -> ADC1 (segun tabla de mapeo RM0383) --- */
    __HAL_RCC_DMA2_CLK_ENABLE();

    hdma_adc1.Instance                 = DMA2_Stream0;
    hdma_adc1.Init.Channel             = DMA_CHANNEL_0;
    hdma_adc1.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc1.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
    hdma_adc1.Init.Mode                = DMA_CIRCULAR;    /* se sobre-escribe solo, sin intervencion del CPU */
    hdma_adc1.Init.Priority            = DMA_PRIORITY_MEDIUM;
    hdma_adc1.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdma_adc1);

    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);

    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

void ACS712_Init(void)
{
    TIM3_Trigger_Init();
    ADC1_DMA_Init();

    /* Arranca el timer de trigger y el ADC en modo DMA circular */
    HAL_TIM_Base_Start(&htim3);
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)acs712_buffer, ACS712_SAMPLES);
}

/* Reconstruye la corriente real (A) a partir de una muestra cruda del ADC,
 * revirtiendo primero el divisor resistivo y luego el offset/sensibilidad del sensor */
float ACS712_RawToAmps(uint16_t raw_adc)
{
    float vadc_mV  = ((float)raw_adc * (float)ADC_VREF_MV) / (float)ADC_MAX_COUNTS;
    float vout_mV  = vadc_mV / ACS712_DIVIDER_RATIO;
    float current_A = (vout_mV - ACS712_ZERO_OFFSET_MV) / (float)ACS712_MV_PER_A;
    return current_A;
}
