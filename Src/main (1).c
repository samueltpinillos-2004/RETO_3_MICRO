#include <stdint.h>
#include "stm32f4xx.h"

// ============================================================================
// 1. CONSTANTES DE CONFIGURACIÓN DEL PWM Y LÍMITES DEL SERVO
// ============================================================================
#define CCR1_MIN   500   // 0 grados  (500 us)
#define CCR1_MAX  2500   // 180 grados (2500 us)
#define PASO_30   333   // Incremento para ~30 grados (2000 us / 6 pasadas)

// Botones K0 y K1 en el Puerto E
#define BTN_K0_PIN  4   // PE4
#define BTN_K1_PIN  3   // PE3

// ============================================================================
// 2. CONFIGURACIÓN DEL GPIO D12 (SALIDA PWM PARA EL SERVO)
// ============================================================================
void PWM_GPIO_Init(void) {
    // 1. Activar el reloj de GPIOD (Bit 3 en AHB1ENR)
    RCC->AHB1ENR |= (1U << 3);

    // 2. Configurar PD12 en Modo Función Alternativa (10 binario en MODER)
    GPIOD->MODER &= ~(3U << (12 * 2));
    GPIOD->MODER |=  (2U << (12 * 2));

    // 3. Configurar tipo de salida en Push-Pull (0 en OTYPER)
    GPIOD->OTYPER &= ~(1U << 12);

    // 4. Configurar velocidad en Alta Velocidad (10 binario en OSPEEDR)
    GPIOD->OSPEEDR &= ~(3U << (12 * 2));
    GPIOD->OSPEEDR |=  (2U << (12 * 2));

    // 5. Configurar sin resistencias internas Pull-up / Pull-down (00 en PUPDR)
    GPIOD->PUPDR &= ~(3U << (12 * 2));

    // 6. Asignar Función Alternativa AF2 (TIM4) a PD12 en AFR[1] (bits 16..19)
    GPIOD->AFR[1] &= ~(0xFU << ((12 - 8) * 4));
    GPIOD->AFR[1] |=  (2U   << ((12 - 8) * 4));
}

// ============================================================================
// 3. CONFIGURACIÓN DEL TIMER 4 (GENERACIÓN PWM A 50 Hz)
// ============================================================================
void PWM_TIM4_Init(void) {
    // 1. Activar reloj del TIM4 en APB1 (Bit 2 = TIM4EN)
    RCC->APB1ENR |= (1U << 2);

    // 2. Prescaler: PSC = 15 -> (16 MHz / 16) = 1 MHz (1 tick = 1 us)
    TIM4->PSC = 15;

    // 3. Auto-Reload: ARR = 19999 -> Periodo de 20 ms (50 Hz)
    TIM4->ARR = 19999;

    // 4. Configurar Modo PWM 1 en Canal 1 (Bits 4, 5 y 6 de CCMR1)
    TIM4->CCMR1 &= ~(7U << 4); // Limpia bits 4..6 (7U = 111)
    TIM4->CCMR1 |=  (6U << 4); // Escribe 110 (Modo PWM 1)

    // 5. Habilitar precarga (Preload) en Canal 1 (Bit 3 = OC1PE)
    TIM4->CCMR1 |= (1U << 3);

    // 6. Asignar ancho de pulso inicial: 1500 us (Posición central / 90°)
    TIM4->CCR1 = 1500;

    // 7. Habilitar la salida física del Canal 1 en PD12 (Bit 0 = CC1E)
    TIM4->CCER |= (1U << 0);

    // 8. Generar evento de actualización para cargar los valores (Bit 0 = UG)
    TIM4->EGR |= (1U << 0);

    // 9. Encender el contador del Timer 4 (Bit 0 = CEN)
    TIM4->CR1 |= (1U << 0);
}

// ============================================================================
// 4. CONFIGURACIÓN DE LOS BOTONES K0 (PE4) Y K1 (PE3)
// ============================================================================
void Buttons_Init(void) {
    // 1. Activar reloj de GPIOE (Bit 4 en AHB1ENR)
    RCC->AHB1ENR |= (1U << 4);

    // 2. Configurar PE3 y PE4 como entradas (00 binario en MODER)
    GPIOE->MODER &= ~((3U << (BTN_K0_PIN * 2)) | (3U << (BTN_K1_PIN * 2)));

    // 3. Activar resistencias de Pull-up internas (01 binario en PUPDR)
    GPIOE->PUPDR &= ~((3U << (BTN_K0_PIN * 2)) | (3U << (BTN_K1_PIN * 2)));
    GPIOE->PUPDR |=  ((1U << (BTN_K0_PIN * 2)) | (1U << (BTN_K1_PIN * 2)));
}

// ============================================================================
// 5. RETARDO POR SOFTWARE Y RUTINA DE ANTIRREBOTE (DEBOUNCE)
// ============================================================================
void Delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms * 4000; i++) {
        __NOP();
    }
}

// Devuelve 1 solo cuando se detecta una pulsación válida y confirmada
uint8_t Debounce_ReadButton(uint8_t pin) {
    // 1. Detecta primer contacto (Pull-up: presionado = '0')
    if (!(GPIOE->IDR & (1U << pin))) {

        Delay_ms(50); // 2. Espera a que los rebotadores mecánicos se estabilicen

        // 3. Reconfirma si el botón sigue estando presionado
        if (!(GPIOE->IDR & (1U << pin))) {

            // 4. Espera de liberación: Bloquea hasta que el usuario suelte el botón
            while (!(GPIOE->IDR & (1U << pin)));

            return 1; // Pulsación única y limpia confirmada
        }
    }
    return 0; // No hubo pulsación válida
}

// ============================================================================
// 6. FUNCIÓN PRINCIPAL (MAIN)
// ============================================================================
int main(void) {
    // Inicialización de periféricos
    PWM_GPIO_Init();
    PWM_TIM4_Init();
    Buttons_Init();

    // Estado inicial: Servomotor centrado a 90° (1500 us)
    uint16_t pulse_width = 1500;
    TIM4->CCR1 = pulse_width;

    while (1) {
        // --- BOTÓN K0 (PE4): DISMINUIR ÁNGULO (-30 GRADOS) ---
        if (Debounce_ReadButton(BTN_K0_PIN)) {
            if (pulse_width >= (CCR1_MIN + PASO_30)) {
                pulse_width -= PASO_30;
            } else {
                pulse_width = CCR1_MIN; // Saturación inferior a 0° (500 us)
            }
            TIM4->CCR1 = pulse_width; // Carga nuevo pulso en el Timer
        }

        // --- BOTÓN K1 (PE3): AUMENTAR ÁNGULO (+30 GRADOS) ---
        if (Debounce_ReadButton(BTN_K1_PIN)) {
            if (pulse_width <= (CCR1_MAX - PASO_30)) {
                pulse_width += PASO_30;
            } else {
                pulse_width = CCR1_MAX; // Saturación superior a 180° (2500 us)
            }
            TIM4->CCR1 = pulse_width; // Carga nuevo pulso en el Timer
        }
    }
}
