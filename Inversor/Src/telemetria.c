#include "telemetria.h"
#include "acs712.h"
#include <stdio.h>


 UART_HandleTypeDef huart2;

/* Configuración del Puerto Serie USART2 (115200 Baudios) */
void UART_Init(void)
{
  __HAL_RCC_USART2_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  // Pines PA2 (TX) y PA3 (RX)
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
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  HAL_UART_Init(&huart2);
}

/* Redireccionar printf a UART */
int _write(int file, char *ptr, int len)
{
  HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, HAL_MAX_DELAY);
  return len;
}

/* Imprime la señal combinada en formato de gráfica ASCII en la consola */
void Telemetria_GraficaASCII(void)
{
  printf("\r\n=== VISUALIZACIÓN DE LA ONDA SPWM EN COOLTERM ===\r\n\r\n");

  for (int i = 0; i < SPWM_SAMPLES; i += 4) // Saltamos de 4 en 4 para escalar la pantalla
  {
    // Obtener valor de cada canal
    int val_ch1 = spwm_lut_ch1[i];
    int val_ch2 = spwm_lut_ch2[i];

    // Convertir a un valor con signo: Positivo (+CH1), Negativo (-CH2)
    int valor_diferencial = val_ch1 - val_ch2;

    // Mapear de [-1000, 1000] a una línea de 50 caracteres
    int posicion_eje = 25; // Centro (0V)
    int barras = (valor_diferencial * 25) / 1000;

    printf("[%03d] |", i);
    for (int j = 0; j < 50; j++)
    {
      if (j == (posicion_eje + barras))
      {
        printf("*"); // Muestra el valor instantáneo
      }
      else if (j == posicion_eje)
      {
        printf("|"); // Línea central de 0V
      }
      else
      {
        printf(" ");
      }
    }
    printf(" | Duty: %d\r\n", valor_diferencial);
  }
  printf("\r\n================================================\r\n");
}

/* Grafica en ASCII la corriente real medida por el ACS712 (buffer circular del DMA).
 * A diferencia de Telemetria_GraficaASCII (que muestra la LUT teorica del PWM),
 * esta funcion muestra la senal YA FILTRADA que realmente llega a la carga,
 * medida en el pin PA0 a traves del divisor resistivo 1k/2.7k. */
void Telemetria_GraficaCorrienteACS712(const uint16_t *buffer, uint16_t len)
{
  printf("\r\n=== CORRIENTE REAL MEDIDA (ACS712 5A, PA0) ===\r\n\r\n");

  for (uint16_t i = 0; i < len; i += 3) /* saltamos de 3 en 3 para no saturar la terminal */
  {
    float amps = ACS712_RawToAmps(buffer[i]);

    /* Mapear rango [-5A, +5A] a una linea de 50 caracteres, centro = 0A */
    int posicion_eje = 25;
    int barras = (int)((amps / 5.0f) * 25.0f);
    if (barras > 25)  barras = 25;
    if (barras < -25) barras = -25;

    printf("[%03u] |", i);
    for (int j = 0; j < 50; j++)
    {
      if (j == (posicion_eje + barras))      printf("*");
      else if (j == posicion_eje)            printf("|");
      else                                   printf(" ");
    }
    printf(" | %.3f A\r\n", amps);
  }
  printf("\r\n================================================\r\n");
}
