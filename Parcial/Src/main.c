/* * =================================================================================
 * Author: Santiago Osorio Huelgos
 *
 *Parcial de taller V
 *
 * INTEGRACIÓN TOTAL: FSM, MPU6050, RTC INTERNO, MCO1 (100 MHz) Y LCD MCO
 * =================================================================================
 * - Direcciones I2C: LCD -> 0x4A | MPU6050 -> 0xD2 (0x69 7-bit)
 * - LED Estado: PH1 vía TIM2 IT (250 ms)
 * - Pin MCO1: PA8 (Salida para Osciloscopio)
 * - Comandos UART (1 sola tecla):
 *      'S' / 's' -> Modo Sensor MPU6050
 *      'T' / 't' -> Modo Reloj RTC
 *      'I' / 'i' -> Modo Reposo (IDLE)
 *      'H' / 'h' -> MCO1 saca HSI -> LCD Muestra "MODE RTC: HSI"
 *      'L' / 'l' -> MCO1 saca LSE -> LCD Muestra "MODE RTC: LSE"
 *      'P' / 'p' -> MCO1 saca PLL -> LCD Muestra "MODE RTC: PLL"
 * =================================================================================
 */

#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* --- DIRECCIONES Y REGISTROS --- */
#define LCD_ADDR             0x4A
#define MPU6050_ADDR         0xD2
#define MPU6050_PWR_MGMT_1   0x6B
#define MPU6050_ACCEL_XOUT_H 0x3B

/* --- DEFINICIÓN DE ESTADOS DE LA FSM --- */
typedef enum {
    STATE_IDLE = 0,
    STATE_SENSOR,
    STATE_RTC
} SystemState_t;

/* --- VARIABLES GLOBALES Y BANDERAS --- */
I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart2;
TIM_HandleTypeDef htim2;
RTC_HandleTypeDef hrtc;

volatile SystemState_t current_state = STATE_IDLE;
volatile uint8_t rx_byte = 0;
volatile uint8_t command_ready = 0;

/* Variable global para guardar el modo de reloj MCO1 actual */
char current_mco_str[8] = "HSI";

/* --- PROTOTIPOS DE FUNCIONES --- */
void SystemClock_Config(void);
void GPIO_Init(void);
void I2C1_Init(void);
void USART2_UART_Init(void);
void TIM2_Init(void);

void MPU6050_Init(void);
void MPU6050_Read_All(int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz);
void Read_and_Display_MPU6050(void);

void LCD_Init(void);
void LCD_SendCommand(uint8_t cmd);
void LCD_SendData(uint8_t data);
void LCD_SendString(char *str);
void LCD_SetCursor(uint8_t row, uint8_t col);
void LCD_Clear(void);

void UART_Print(char *str);
void I2C_Scanner(void);
void Process_UART_Command(uint8_t cmd);

void RTC_Init(void);
void Read_and_Display_RTC(void);

/* =================================================================================
 * INTERRUPCIONES DE HARDWARE
 * ================================================================================= */

// 1. Interrupción de Temporizador: LED de Estado en PH1 (250 ms)
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        HAL_GPIO_TogglePin(GPIOH, GPIO_PIN_1);
    }
}

// 2. Interrupción de Recepción UART
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        command_ready = 1; // Avisa al main que llegó una tecla
    }
}

/* =================================================================================
 * PROGRAMA PRINCIPAL
 * ================================================================================= */
int main(void)
{
    HAL_Init();

    /* Configura el procesador a 100 MHz */
    SystemClock_Config();

    /* Inicializar periféricos */
    GPIO_Init();
    USART2_UART_Init();
    TIM2_Init();
    I2C1_Init();
    RTC_Init();

    UART_Print("\r\n==============================================\r\n");
    UART_Print("   EXAMEN TALLER V \r\n");
    UART_Print("==============================================\r\n\r\n");

    /* Activar temporizador TIM2 e interrupción UART */
    HAL_TIM_Base_Start_IT(&htim2);
    HAL_UART_Receive_IT(&huart2, (uint8_t*)&rx_byte, 1);

    /* Inicializar I2C, LCD y MPU6050 */
    I2C_Scanner();
    LCD_Init();
    MPU6050_Init();

    /* Estado Inicial IDLE */
    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_SendString("   SYSTEM READY   ");
    LCD_SetCursor(1, 0);
    LCD_SendString("   STATE: IDLE    ");

    while (1)
    {
        // -------------------------------------------------------------------------
        // A) MANEJO DE TECLAS Y COMANDOS (TRANSICIONES Y RELOJ MCO1)
        // -------------------------------------------------------------------------
        if (command_ready)
        {
            command_ready = 0;
            Process_UART_Command(rx_byte);

            // Reactivar recepción UART por interrupción para la siguiente tecla
            HAL_UART_Receive_IT(&huart2, (uint8_t*)&rx_byte, 1);
        }

        // -------------------------------------------------------------------------
        // B) EJECUCIÓN DEL ESTADO ACTUAL DE LA FSM
        // -------------------------------------------------------------------------
        switch (current_state)
        {
            case STATE_IDLE:
                break;

            case STATE_SENSOR:
                Read_and_Display_MPU6050();
                HAL_Delay(200); // Refresco ~5Hz
                break;

            case STATE_RTC:
                Read_and_Display_RTC();
                HAL_Delay(1000); // Refresco 1 Hz
                break;
        }
    }
}

/* =================================================================================
 * PROCESADOR DE COMANDOS UART (FSM + MCO1)
 * ================================================================================= */
void Process_UART_Command(uint8_t cmd)
{
    switch (cmd)
    {
        /* --- COMANDOS DE CAMBIO DE ESTADO FSM --- */
        case 'I': case 'i':
            current_state = STATE_IDLE;
            LCD_Clear();
            LCD_SetCursor(0, 0);
            LCD_SendString("   SYSTEM READY   ");
            LCD_SetCursor(1, 0);
            LCD_SendString("   STATE: IDLE    ");
            UART_Print("\r\n[FSM] Modo: IDLE\r\n");
            break;

        case 'S': case 's':
            current_state = STATE_SENSOR;
            LCD_Clear();
            UART_Print("\r\n[FSM] Modo: SENSOR (MPU6050)\r\n");
            break;

        case 'T': case 't':
            current_state = STATE_RTC;
            LCD_Clear();
            UART_Print("\r\n[FSM] Modo: RELOJ RTC\r\n");
            break;

        /* --- COMANDOS PARA MCO1 (OSCILOSCOPIO EN PA8) --- */
        case 'H': case 'h':
            HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSI, RCC_MCODIV_5);
            strcpy(current_mco_str, "HSI"); // Actualiza nombre para el LCD
            UART_Print("\r\n[MCO1] Salida PA8 -> HSI (3.2 MHz)\r\n");
            break;

        case 'L': case 'l':
            HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_LSE, RCC_MCODIV_1);
            strcpy(current_mco_str, "LSE"); // Actualiza nombre para el LCD
            UART_Print("\r\n[MCO1] Salida PA8 -> LSE (32.768 kHz)\r\n");
            break;

        case 'P': case 'p':
            HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_PLLCLK, RCC_MCODIV_5);
            strcpy(current_mco_str, "PLL"); // Actualiza nombre para el LCD
            UART_Print("\r\n[MCO1] Salida PA8 -> PLL (20.0 MHz = Nucleo 100MHz)\r\n");
            break;

        default:
            UART_Print("\r\n[AYUDA] Teclas: 'S' (Sensor), 'T' (RTC), 'I' (Idle) | MCO1: 'H', 'L', 'P'\r\n");
            break;
    }
}

/* =================================================================================
 * FUNCIÓN RTC CON VISUALIZACIÓN DE FUENTE MCO EN LA LÍNEA 4 DEL LCD
 * ================================================================================= */
void Read_and_Display_RTC(void)
{
    RTC_TimeTypeDef gTime = {0};
    RTC_DateTypeDef gDate = {0};
    char buffer_lcd[24];
    char buffer_uart[128];

    HAL_RTC_WaitForSynchro(&hrtc);
    HAL_RTC_GetTime(&hrtc, &gTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &gDate, RTC_FORMAT_BIN);

    // Fila 0: Encabezado
    LCD_SetCursor(0, 0); LCD_SendString("=== REAL TIME CLK ==");

    // Fila 1: Hora (HH:MM:SS)
    snprintf(buffer_lcd, sizeof(buffer_lcd), "HORA:  %02d:%02d:%02d   ", gTime.Hours, gTime.Minutes, gTime.Seconds);
    LCD_SetCursor(1, 0); LCD_SendString(buffer_lcd);

    // Fila 2: Fecha (DD/MM/AAAA)
    snprintf(buffer_lcd, sizeof(buffer_lcd), "FECHA: %02d/%02d/20%02d ", gDate.Date, gDate.Month, gDate.Year);
    LCD_SetCursor(2, 0); LCD_SendString(buffer_lcd);

    // Fila 3: Modo MCO1 seleccionado ("MODE RTC: HSI", "MODE RTC: LSE", "MODE RTC: PLL")
    snprintf(buffer_lcd, sizeof(buffer_lcd), "MODE RTC: %-7s", current_mco_str);
    LCD_SetCursor(3, 0); LCD_SendString(buffer_lcd);

    // Salida por consola UART
    snprintf(buffer_uart, sizeof(buffer_uart), "[RTC] HORA: %02d:%02d:%02d | FECHA: %02d/%02d/20%02d | MCO1: %s\r\n",
             gTime.Hours, gTime.Minutes, gTime.Seconds, gDate.Date, gDate.Month, gDate.Year, current_mco_str);
    UART_Print(buffer_uart);
}

/* =================================================================================
 * CONFIGURACIÓN DE RELOJ A 100 MHz Y PIN PA8 (MCO1)
 * ================================================================================= */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 16;
    RCC_OscInitStruct.PLL.PLLN = 200;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3);
}

void GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // LED en PH1
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

    // Pin PA8 -> Salida MCO1
    GPIO_InitStruct.Pin = GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF0_MCO;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/* =================================================================================
 * RESTO DE FUNCIONES DE APOYO
 * ================================================================================= */
void Read_and_Display_MPU6050(void)
{
    // Usamos variables estáticas para recordar el último valor válido si hay un fallo
    static int16_t ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
    int16_t temp_ax, temp_ay, temp_az, temp_gx, temp_gy, temp_gz;

    char buffer_lcd[24];
    char buffer_uart[128];

    // Intentamos leer
    temp_ax = ax; temp_ay = ay; temp_az = az;
    temp_gx = gx; temp_gy = gy; temp_gz = gz;

    MPU6050_Read_All(&temp_ax, &temp_ay, &temp_az, &temp_gx, &temp_gy, &temp_gz);

    // Si la lectura devolvió datos válidos (distintos de cero absoluto en los 3 ejes a la vez)
    if (temp_ax != 0 || temp_ay != 0 || temp_az != 0)
    {
        ax = temp_ax; ay = temp_ay; az = temp_az;
        gx = temp_gx; gy = temp_gy; gz = temp_gz;
    }

    // Conversión a mg y dps
    int32_t ax_mg = ((int32_t)ax * 1000) / 16384;
    int32_t ay_mg = ((int32_t)ay * 1000) / 16384;
    int32_t az_mg = ((int32_t)az * 1000) / 16384;
    int16_t gz_d = gz / 131;

    // Actualizar LCD
    snprintf(buffer_lcd, sizeof(buffer_lcd), "ACC X: %s%d.%03d g", (ax_mg < 0) ? "-" : " ", (int)(abs(ax_mg) / 1000), (int)(abs(ax_mg) % 1000));
    LCD_SetCursor(0, 0); LCD_SendString(buffer_lcd);

    snprintf(buffer_lcd, sizeof(buffer_lcd), "ACC Y: %s%d.%03d g", (ay_mg < 0) ? "-" : " ", (int)(abs(ay_mg) / 1000), (int)(abs(ay_mg) % 1000));
    LCD_SetCursor(1, 0); LCD_SendString(buffer_lcd);

    snprintf(buffer_lcd, sizeof(buffer_lcd), "ACC Z: %s%d.%03d g", (az_mg < 0) ? "-" : " ", (int)(abs(az_mg) / 1000), (int)(abs(az_mg) % 1000));
    LCD_SetCursor(2, 0); LCD_SendString(buffer_lcd);

    snprintf(buffer_lcd, sizeof(buffer_lcd), "GYR Z: %s%d dps      ", (gz_d < 0) ? "-" : " ", abs(gz_d));
    LCD_SetCursor(3, 0); LCD_SendString(buffer_lcd);

    // Actualizar UART
    snprintf(buffer_uart, sizeof(buffer_uart), "[MPU6050] ACC [g]: X=%s%d.%03d | Y=%s%d.%03d | Z=%s%d.%03d || GYR Z: %d dps\r\n",
             (ax_mg < 0) ? "-" : "", (int)(abs(ax_mg) / 1000), (int)(abs(ax_mg) % 1000),
             (ay_mg < 0) ? "-" : "", (int)(abs(ay_mg) / 1000), (int)(abs(ay_mg) % 1000),
             (az_mg < 0) ? "-" : "", (int)(abs(az_mg) / 1000), (int)(abs(az_mg) % 1000), gz_d);
    UART_Print(buffer_uart);
}
void MPU6050_Init(void)
{
    uint8_t data = 0x00;
    if (HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, MPU6050_PWR_MGMT_1, 1, &data, 1, 100) == HAL_OK)
        UART_Print("[OK] Sensor MPU6050 activado.\r\n");
    else
        UART_Print("[ERROR] Fallo I2C MPU6050!\r\n");
}

void MPU6050_Read_All(int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz)
{
    uint8_t d[14];
    HAL_StatusTypeDef status;

    // 1. Desactivar interrupción UART brevemente para proteger la transacción I2C
    HAL_NVIC_DisableIRQ(USART2_IRQn);

    // 2. Intentar leer los 14 registros del MPU6050
    status = HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, MPU6050_ACCEL_XOUT_H, 1, d, 14, 100);

    // 3. Reactivar la interrupción UART inmediatamente
    HAL_NVIC_EnableIRQ(USART2_IRQn);

    if (status == HAL_OK)
    {
        *ax = (int16_t)(d[0] << 8 | d[1]);
        *ay = (int16_t)(d[2] << 8 | d[3]);
        *az = (int16_t)(d[4] << 8 | d[5]);
        *gx = (int16_t)(d[8] << 8 | d[9]);
        *gy = (int16_t)(d[10] << 8 | d[11]);
        *gz = (int16_t)(d[12] << 8 | d[13]);
    }
    else
    {
        // Si ocurrió una falla por cambio de reloj o interrupción:
        // Intentamos 'despertar' nuevamente al MPU6050 y resetear el bus I2C
        uint8_t pwr_mgmt = 0x00;
        HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, MPU6050_PWR_MGMT_1, 1, &pwr_mgmt, 1, 50);
    }
}

void RTC_Init(void)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE | RCC_OSCILLATORTYPE_LSI;
    RCC_OscInitStruct.LSEState = RCC_LSE_ON;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) == HAL_OK)
    {
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
        PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
        HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);
    }
    else
    {
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
        PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
        HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);
    }

    __HAL_RCC_RTC_ENABLE();

    hrtc.Instance = RTC;
    hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv = 127;
    hrtc.Init.SynchPrediv = 255;
    hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
    HAL_RTC_Init(&hrtc);

    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) != 0x32F2)
    {
        UART_Print("[RTC] Primera configuracion. Ajustando hora por defecto...\r\n");
        sTime.Hours = 12; sTime.Minutes = 0; sTime.Seconds = 0;
        HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
        sDate.WeekDay = RTC_WEEKDAY_FRIDAY; sDate.Month = RTC_MONTH_JULY; sDate.Date = 24; sDate.Year = 26;
        HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, 0x32F2);
    }
    else
    {
        UART_Print("[RTC] Hora recuperada de la memoria de respaldo (VBAT Activa).\r\n");
    }
}

void LCD_SendCommand(uint8_t cmd)
{
    uint8_t data_u = (cmd & 0xF0); uint8_t data_l = ((cmd << 4) & 0xF0);
    uint8_t data_t[4] = { data_u | 0x0C, data_u | 0x08, data_l | 0x0C, data_l | 0x08 };
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, data_t, 4, 100);
}

void LCD_SendData(uint8_t data)
{
    uint8_t data_u = (data & 0xF0); uint8_t data_l = ((data << 4) & 0xF0);
    uint8_t data_t[4] = { data_u | 0x0D, data_u | 0x09, data_l | 0x0D, data_l | 0x09 };
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, data_t, 4, 100);
}

void LCD_Init(void)
{
    HAL_Delay(100);
    LCD_SendCommand(0x30); HAL_Delay(10); LCD_SendCommand(0x30); HAL_Delay(5);
    LCD_SendCommand(0x30); HAL_Delay(5);  LCD_SendCommand(0x20); HAL_Delay(5);
    LCD_SendCommand(0x28); HAL_Delay(2);  LCD_SendCommand(0x0C); HAL_Delay(2);
    LCD_SendCommand(0x06); HAL_Delay(2);  LCD_SendCommand(0x01); HAL_Delay(5);
}

void LCD_Clear(void) { LCD_SendCommand(0x01); HAL_Delay(2); }

void LCD_SetCursor(uint8_t row, uint8_t col)
{
    uint8_t row_offsets[] = {0x00, 0x40, 0x14, 0x54};
    if (row > 3) row = 3;
    LCD_SendCommand(0x80 | (col + row_offsets[row]));
}

void LCD_SendString(char *str) { while (*str) LCD_SendData(*str++); }

void I2C_Scanner(void)
{
    char msg[64];
    UART_Print("--- ESCANEANDO BUS I2C1 ---\r\n");
    for (uint16_t i = 1; i < 128; i++)
    {
        if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(i << 1), 1, 10) == HAL_OK)
        {
            snprintf(msg, sizeof(msg), "Dispositivo -> 7-bit: 0x%02X | HAL (8-bit): 0x%02X\r\n", i, (i << 1));
            UART_Print(msg);
        }
    }
    UART_Print("---------------------------\r\n\r\n");
}

void UART_Print(char *str) { HAL_UART_Transmit(&huart2, (uint8_t*)str, strlen(str), 100); }

void TIM2_Init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 9999;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 2499;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_Base_Init(&htim2);
    HAL_NVIC_SetPriority(TIM2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
}

void I2C1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    __HAL_RCC_I2C1_CLK_ENABLE();
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    HAL_I2C_Init(&hi2c1);
}

void USART2_UART_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    HAL_UART_Init(&huart2);

    HAL_NVIC_SetPriority(USART2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}
