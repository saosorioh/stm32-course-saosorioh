/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Santiago / Adaptado a Timers
 * @brief          : Conteo con sensores ópticos y display multiplexado usando TIM3
 ******************************************************************************
 */

#include <stdint.h>
#include <stm32f4xx.h>

// Variables Globales //
volatile uint16_t contador_general = 0;		// Valor de conteo actual //
volatile uint8_t sensor_suma_estable = 0;	// Validando al sensor 1 (sensor suma) //
volatile uint8_t sensor_resta_estable = 0;	// Validando al sensor 2 (sensor resta) //
uint8_t tim2_flag = 0;

// Memorias de flanco para que sumen o resten una vez //
uint8_t memoria_suma = 0;
uint8_t memoria_resta = 0;

// Contadores individuales de tiempo para evitar rebotes //
uint8_t tiempo_filtro_suma = 0;
uint8_t tiempo_filtro_resta = 0;

// Variables de descomposición del número del display //
uint8_t digito_millar = 0;
uint8_t digito_centena = 0;
uint8_t digito_decena = 0;
uint8_t digito_unidad = 0;

volatile uint8_t posicion_display = 0;	// Controla el digito del display //

// Prototipos de Funciones //
void Iniciar_Relojes(void);
void Configurar_Pines_Control(void);
void Configurar_Pines_Segmentos(void);
void Configurar_Pin_Led(void);
void Enviar_Segmentos(uint8_t numero);
void Apagar_Todos_Los_Digitos(void);
void init_timer2_led(void);      // Temporizador para el LED de estado
void init_timer3_display(void);  // Nuevo: Temporizador para Display y Sensores (2ms)
void TIM2_IRQHandler(void);
void TIM3_IRQHandler(void);      // Nueva: Rutina de Interrupción del TIM3

// Funciones principales //
int main(void) {
	Iniciar_Relojes();
	Configurar_Pines_Control();
	Configurar_Pines_Segmentos();
	Configurar_Pin_Led();
	init_timer2_led();
	init_timer3_display(); // Inicializamos el TIM3 en reemplazo del SysTick

	while (1) {

		if(tim2_flag == 1){
			tim2_flag = 0;
			GPIOH->ODR ^= GPIO_ODR_OD1; // Led de estado conmuta en PH1
		}

		// CAPA DE PROCESAMIENTO: DESCOMPOSICIÓN MATEMÁTICA //
		uint16_t temp = contador_general;

		digito_millar  = temp / 1000;	// Primer digito //
		temp           = temp % 1000;

		digito_centena = temp / 100;	// Segundo digito //
		temp           = temp % 100;

		digito_decena  = temp / 10;		// Tercer digito //
		digito_unidad  = temp % 10;
	}
}

		//Control de tiempo//

void TIM3_IRQHandler(void) {
	// Verificamos que la interrupción sea por el evento de actualización (Update Interrupt Flag)
	if (TIM3->SR & TIM_SR_UIF) {
		TIM3->SR &= ~(TIM_SR_UIF); // Limpieza obligatoria de la bandera por software

		// --- FILTRO SENSOR 1 DE SUMA ---
		if ((GPIOC->IDR & GPIO_IDR_IDR_10) == 0) {
			if (tiempo_filtro_suma < 5) {
				tiempo_filtro_suma++;
			} else {
				sensor_suma_estable = 1;
			}
		} else {
			tiempo_filtro_suma = 0;
			sensor_suma_estable = 0;
		}

		// --- LÓGICA DE CONTEO POR FLANCO (SUMA) ---
		if (sensor_suma_estable == 1) {
			if (memoria_suma == 0) {
				if (contador_general == 9999) {
					contador_general = 0;
				} else {
					contador_general++;
				}
				memoria_suma = 1;
			}
		} else {
			memoria_suma = 0;
		}

		// --- FILTRO SENSOR DE RESTA ---
		if ((GPIOC->IDR & GPIO_IDR_IDR_12) == 0) {
			if (tiempo_filtro_resta < 5) {
				tiempo_filtro_resta++;
			} else {
				sensor_resta_estable = 1;
			}
		} else {
			tiempo_filtro_resta = 0;
			sensor_resta_estable = 0;
		}

		// --- LÓGICA DE CONTEO POR FLANCO (RESTA) ---
		if (sensor_resta_estable == 1) {
			if (memoria_resta == 0) {
				if (contador_general == 0) {
					contador_general = 9999;
				} else {
					contador_general--;
				}
				memoria_resta = 1;
			}
		} else {
			memoria_resta = 0;
		}

		// --- MULTIPLEXADO DEL DISPLAY ---
		Apagar_Todos_Los_Digitos();

		switch (posicion_display) {
		case 0:
			Enviar_Segmentos(digito_millar);
			GPIOC->BSRR = GPIO_BSRR_BR11;
			posicion_display = 1;
			break;
		case 1:
			Enviar_Segmentos(digito_centena);
			GPIOD->BSRR = GPIO_BSRR_BR2;
			posicion_display = 2;
			break;
		case 2:
			Enviar_Segmentos(digito_decena);
			GPIOB->BSRR = GPIO_BSRR_BR15;
			posicion_display = 3;
			break;
		case 3:
			Enviar_Segmentos(digito_unidad);
			GPIOB->BSRR = GPIO_BSRR_BR10;
			posicion_display = 0;
			break;
		}
	}
}

// Pasar de numero a segmentos //
void Enviar_Segmentos(uint8_t numero) {
	GPIOA->BSRR = (GPIO_BSRR_BS0 | GPIO_BSRR_BS1 | GPIO_BSRR_BS4 | GPIO_BSRR_BS10);
	GPIOB->BSRR = (GPIO_BSRR_BS5 | GPIO_BSRR_BS14);
	GPIOC->BSRR = GPIO_BSRR_BS4;

	switch(numero) {
	case 0: GPIOA->BSRR = (GPIO_BSRR_BR0 | GPIO_BSRR_BR1 | GPIO_BSRR_BR4 | GPIO_BSRR_BR10); GPIOB->BSRR = GPIO_BSRR_BR5; GPIOC->BSRR = GPIO_BSRR_BR4; break;
	case 1: GPIOA->BSRR = GPIO_BSRR_BR4; GPIOB->BSRR = GPIO_BSRR_BR5; break;
	case 2: GPIOA->BSRR = (GPIO_BSRR_BR0 | GPIO_BSRR_BR4 | GPIO_BSRR_BR10); GPIOB->BSRR = GPIO_BSRR_BR14; GPIOC->BSRR = GPIO_BSRR_BR4; break;
	case 3: GPIOA->BSRR = (GPIO_BSRR_BR0 | GPIO_BSRR_BR4 | GPIO_BSRR_BR10); GPIOB->BSRR = (GPIO_BSRR_BR5 | GPIO_BSRR_BR14); break;
	case 4: GPIOA->BSRR = (GPIO_BSRR_BR1 | GPIO_BSRR_BR4); GPIOB->BSRR = (GPIO_BSRR_BR5 | GPIO_BSRR_BR14); break;
	case 5: GPIOA->BSRR = (GPIO_BSRR_BR0 | GPIO_BSRR_BR1 | GPIO_BSRR_BR10); GPIOB->BSRR = (GPIO_BSRR_BR5 | GPIO_BSRR_BR14); break;
	case 6: GPIOA->BSRR = (GPIO_BSRR_BR0 | GPIO_BSRR_BR1 | GPIO_BSRR_BR10); GPIOB->BSRR = (GPIO_BSRR_BR5 | GPIO_BSRR_BR14); GPIOC->BSRR = GPIO_BSRR_BR4; break;
	case 7: GPIOA->BSRR = (GPIO_BSRR_BR0 | GPIO_BSRR_BR4); GPIOB->BSRR = GPIO_BSRR_BR5; break;
	case 8: GPIOA->BSRR = (GPIO_BSRR_BR0 | GPIO_BSRR_BR1 | GPIO_BSRR_BR4 | GPIO_BSRR_BR10); GPIOB->BSRR = (GPIO_BSRR_BR5 | GPIO_BSRR_BR14); GPIOC->BSRR = GPIO_BSRR_BR4; break;
	case 9: GPIOA->BSRR = (GPIO_BSRR_BR0 | GPIO_BSRR_BR1 | GPIO_BSRR_BR4 | GPIO_BSRR_BR10); GPIOB->BSRR = (GPIO_BSRR_BR5 | GPIO_BSRR_BR14); break;
	}
}

// Efecto antifantasma, apaga el display //
void Apagar_Todos_Los_Digitos(void) {
	GPIOC->BSRR = GPIO_BSRR_BS11;
	GPIOD->BSRR = GPIO_BSRR_BS2;
	GPIOB->BSRR = GPIO_BSRR_BS15;
	GPIOB->BSRR = GPIO_BSRR_BS10;
}

// Activacion de los relojes de los perifericos //
void Iniciar_Relojes(void) {
	// Relojes de Puertos A, B, C, D y H activos
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_GPIOHEN;
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
}

// Configuracion de hardware para sensores y transistores //
void Configurar_Pines_Control(void) {
	// Configuracion de los sensores como entrada (limpieza moder)
	GPIOC->MODER &= ~(GPIO_MODER_MODER10 | GPIO_MODER_MODER12);

	// Activa resistencias de PULL-UP internas de los sensores //
	GPIOC->PUPDR &= ~(GPIO_PUPDR_PUPDR10 | GPIO_PUPDR_PUPDR12);
	GPIOC->PUPDR |=  (GPIO_PUPDR_PUPDR10_0 | GPIO_PUPDR_PUPDR12_0);

	// Configura los transistores como salidas //
	GPIOC->MODER &= ~(GPIO_MODER_MODER11); GPIOC->MODER |= GPIO_MODER_MODER11_0;
	GPIOD->MODER &= ~(GPIO_MODER_MODER2);  GPIOD->MODER |= GPIO_MODER_MODER2_0;
	GPIOB->MODER &= ~(GPIO_MODER_MODER10 | GPIO_MODER_MODER15);
	GPIOB->MODER |= (GPIO_MODER_MODER10_0 | GPIO_MODER_MODER15_0);
}

// Configuración de hardware para el LED de Estado en PH1 //
void Configurar_Pin_Led(void) {
	GPIOH->MODER &= ~(GPIO_MODER_MODER1);
	GPIOH->MODER |= GPIO_MODER_MODER1_0;
}

// Configuracion del hardware para los 7 segmentos //
void Configurar_Pines_Segmentos(void) {
	GPIOA->MODER &= ~(GPIO_MODER_MODER0 | GPIO_MODER_MODER1 | GPIO_MODER_MODER4 | GPIO_MODER_MODER10);
	GPIOA->MODER |=  (GPIO_MODER_MODER0_0 | GPIO_MODER_MODER1_0 | GPIO_MODER_MODER4_0 | GPIO_MODER_MODER10_0);
	GPIOB->MODER &= ~(GPIO_MODER_MODER5 | GPIO_MODER_MODER14);
	GPIOB->MODER |=  (GPIO_MODER_MODER5_0 | GPIO_MODER_MODER14_0);
	GPIOC->MODER &= ~(GPIO_MODER_MODER4);
	GPIOC->MODER |=  (GPIO_MODER_MODER4_0);
}

// Inicializa el TIM2 para el LED (Genera evento cada 500ms) //
void init_timer2_led(void){
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
	TIM2->CR1 &= ~(0b1 << TIM_CR1_DIR_Pos);
	TIM2->CR1 &= ~(0b1 << TIM_CR1_ARPE_Pos);
	TIM2->DIER |= (0b1 << TIM_DIER_UIE_Pos);
	TIM2->SR &= ~(0b1 << TIM_SR_UIF_Pos);
	TIM2->CNT = 0;
	TIM2->PSC = 15999;
	TIM2->ARR = 499;
	NVIC_EnableIRQ(TIM2_IRQn);
	TIM2->CR1 |= (0b1 << TIM_CR1_CEN_Pos);
}

// Inicializa el TIM3 para el Display y Filtros (Genera evento cada 2ms) //
void init_timer3_display(void){
	// 1. Activar el reloj del periférico TIM3 en el bus APB1
	RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

	// 2. Configurar dirección de conteo hacia arriba (Upcounting)
	TIM3->CR1 &= ~(0b1 << TIM_CR1_DIR_Pos);

	// 3. Desactivar el autoreload precargado (Normal update)
	TIM3->CR1 &= ~(0b1 << TIM_CR1_ARPE_Pos);

	// 4. Habilitar la interrupción por actualización (Update Interrupt Enable)
	TIM3->DIER |= (0b1 << TIM_DIER_UIE_Pos);

	// 5. Limpiar bandera de estado por seguridad antes de arrancar
	TIM3->SR &= ~(0b1 << TIM_SR_UIF_Pos);

	// 6. Resetear el valor inicial del contador interno
	TIM3->CNT = 0;

	// 7. Prescaler: Reduce la señal de 16MHz a 1kHz (16000000 / 16000 = 1000 Hz)
	TIM3->PSC = 15999;

	// 8. Auto-Reload: Cuenta del 0 al 1. Como cuenta dos pasos a 1kHz,
	// genera un disparo exactamente cada 2ms (Frecuencia de 500 Hz).
	TIM3->ARR = 1;

	// 9. Activar la interrupción del TIM3 en el controlador de vectores del núcleo ARM (NVIC)
	NVIC_EnableIRQ(TIM3_IRQn);

	// 10. Encender el Timer 3 (Counter Enable)
	TIM3->CR1 |= (0b1 << TIM_CR1_CEN_Pos);
}

// Manejador de la interrupción de TIM2 (LED) //
void TIM2_IRQHandler(void){
	if(TIM2->SR && TIM_SR_UIF){
		TIM2->SR &= ~(TIM_SR_UIF);
		tim2_flag = 1;
	}
}
