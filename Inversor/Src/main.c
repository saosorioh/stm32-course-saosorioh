/*
 * =================================================================================
 * PROYECTO: INVERSOR SPWM 12V->24V con L298N, filtro LC, ACS712 5A y telemetría
 * =================================================================================
 * - PA8  -> TIM1_CH1 (IN1 L298N)   | PA9 -> TIM1_CH2 (IN2 L298N)   [ENA con jumper]
 * - PA2/PA3 -> USART2 TX/RX (115200 bps)
 * - PA0  -> ADC1 (salida ACS712, PENDIENTE de integrar en este entregable)
 * - SystemClock: HSI(16MHz) -> PLL -> 96MHz (SYSCLK=HCLK=96MHz, APB1=48MHz, APB2=96MHz)
 * - TIM1 cuelga de APB2 (sin duplicación x2 porque APB2 prescaler = /1) -> f_TIM1 = 96MHz
 *
 * Comando UART (3 caracteres, ej. "060" = 60 Hz):
 *   Se recibe por interrupción, se valida rango 45-65 Hz, y se recalcula ARR de TIM1
 *   en caliente, deteniendo y reiniciando el DMA circular de forma segura.
 * =================================================================================
 */

#include "stm32f4xx_hal.h"
#include "spwm_config.h"
#include "spwm_lut.h"
#include "telemetria.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* --- LÍMITES DE SEGURIDAD PARA LA FRECUENCIA DE SALIDA --- */
#define F_OUT_MIN   45u
#define F_OUT_MAX   65u
#define F_TIM1_HZ   96000000UL   /* Reloj real de TIM1 tras SystemClock_Config */
#define PSC_FIJO    3u           /* PSC+1 = 4, igual al diseño original */

/* --- BUFFER DE RECEPCIÓN UART (comando de 3 caracteres) --- */
volatile uint8_t rx_cmd[3];
volatile uint8_t rx_index = 0;
volatile uint8_t cmd_ready = 0;

/* --- PROTOTIPOS --- */
void SystemClock_Config(void);
static void Set_Output_Frequency(uint16_t freq_hz);
static void Restart_UART_RxIT(void);

/* =================================================================================
 * INTERRUPCIÓN DE RECEPCIÓN UART — arma 3 caracteres antes de avisar al main
 * ================================================================================= */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        rx_index++;
        if (rx_index >= 3)
        {
            cmd_ready = 1;
            rx_index = 0;
        }
    }
}

/* =================================================================================
 * PROGRAMA PRINCIPAL
 * ================================================================================= */
int main(void)
{
    HAL_Init();

    /* Reloj del sistema a 96 MHz (HSI -> PLL), calculado para dar 60 Hz exactos
     * en la salida SPWM con SPWM_SAMPLES=334 y PSC=3 */
    SystemClock_Config();

    /* Periféricos */
    UART_Init();      /* telemetria.c: USART2 @115200 */
    SPWM_Init();       /* spwm_config.c: TIM1 CH1/CH2 + DMA2 circular */

    printf("\r\n==================================================\r\n");
    printf("   INVERSOR SPWM - 12V->24V - L298N - ACS712 5A\r\n");
    printf("==================================================\r\n");
    printf("Ingrese 3 digitos para frecuencia (045-065), ej: 060\r\n\r\n");

    /* Arranque en 60 Hz por defecto */
    Set_Output_Frequency(60);
    SPWM_Start();

    /* Armamos la recepción del primer byte por interrupción */
    HAL_UART_Receive_IT(&huart2, (uint8_t*)&rx_cmd[0], 1);
    Restart_UART_RxIT();

    while (1)
    {
        if (cmd_ready)
        {
            cmd_ready = 0;

            /* Validar que sean 3 dígitos ASCII */
            if (rx_cmd[0] >= '0' && rx_cmd[0] <= '9' &&
                rx_cmd[1] >= '0' && rx_cmd[1] <= '9' &&
                rx_cmd[2] >= '0' && rx_cmd[2] <= '9')
            {
                uint16_t freq = (rx_cmd[0]-'0')*100 + (rx_cmd[1]-'0')*10 + (rx_cmd[2]-'0');

                if (freq >= F_OUT_MIN && freq <= F_OUT_MAX)
                {
                    Set_Output_Frequency(freq);
                    printf("[OK] Nueva frecuencia: %u Hz\r\n", freq);
                }
                else
                {
                    printf("[ERROR] Rango valido: %u-%u Hz\r\n", F_OUT_MIN, F_OUT_MAX);
                }
            }
            else
            {
                printf("[ERROR] Formato invalido. Use 3 digitos, ej: 060\r\n");
            }

            /* Rearmar recepción para el siguiente comando de 3 caracteres */
            HAL_UART_Receive_IT(&huart2, (uint8_t*)&rx_cmd[0], 1);
        }

        /* Aquí se integrará: lectura ADC (ACS712) + actualización OLED */
    }
}

/* =================================================================================
 * Recalcula ARR de TIM1 para la frecuencia de salida deseada, reiniciando el DMA
 * circular de forma segura (sin dejar el puente H en un estado intermedio raro)
 * ================================================================================= */
static void Set_Output_Frequency(uint16_t freq_hz)
{
    /* ARR_nuevo = round( F_TIM1 / ((PSC+1) * SPWM_SAMPLES * freq_hz) ) - 1 */
    uint32_t denom = (uint32_t)(PSC_FIJO + 1) * SPWM_SAMPLES * freq_hz;
    uint32_t arr_mas_1 = (F_TIM1_HZ + (denom / 2)) / denom;  /* redondeo */
    uint32_t nuevo_arr = arr_mas_1 - 1;

    /* Detener DMA/PWM antes de tocar ARR, para no dejar un pulso a medio generar */
    HAL_TIM_PWM_Stop_DMA(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop_DMA(&htim1, TIM_CHANNEL_2);

    __HAL_TIM_SET_AUTORELOAD(&htim1, nuevo_arr);
    __HAL_TIM_SET_COUNTER(&htim1, 0);

    /* Reiniciar las transferencias circulares desde el inicio de la LUT */
    HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_1, (uint32_t *)spwm_lut_ch1, SPWM_SAMPLES);
    HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_2, (uint32_t *)spwm_lut_ch2, SPWM_SAMPLES);
}

static void Restart_UART_RxIT(void)
{
    rx_index = 0;
    cmd_ready = 0;
}

/* =================================================================================
 * SystemClock_Config: HSI(16MHz) -> PLL -> 96MHz
 * Calculado específicamente para que TIM1 (APB2, sin división) corra a 96MHz,
 * dando 59.98Hz de salida con PSC=3, ARR=1197, SPWM_SAMPLES=334 (ver justificación
 * matemática ya discutida: error de ~0.03%, indistinguible en la práctica).
 * ================================================================================= */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource        = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM            = 16;             /* 16MHz / 16 = 1MHz */
    RCC_OscInitStruct.PLL.PLLN            = 192;             /* 1MHz x 192 = 192MHz */
    RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV2;   /* 192MHz / 2 = 96MHz */
    RCC_OscInitStruct.PLL.PLLQ            = 4;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        while (1) { __NOP(); }
    }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                                        RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK  = 96 MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;     /* APB1  = 48 MHz (max 50MHz) */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;     /* APB2  = 96 MHz (max 100MHz), TIM1 vive aquí */

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
    {
        while (1) { __NOP(); }
    }
}
