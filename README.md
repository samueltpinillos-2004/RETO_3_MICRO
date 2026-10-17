# Control PWM para Servomotor en STM32F407 (Bare-Metal)

Implementación académica a nivel de registros (*bare-metal*) para el control preciso de un servomotor utilizando el microcontrolador **STM32F407**. Este proyecto evita el uso de librerías de abstracción de alto nivel (como HAL), empleando manipulación directa de registros y operaciones a nivel de bits para configurar temporizadores de hardware, puertos GPIO y lógica de control.

---

## 1. Descripción del Módulo y Protocolo Desarrollado

### Resumen del Sistema
El proyecto implementa un generador de **PWM (Modulación por Ancho de Pulso)** controlado por hardware, complementado con una interfaz de pulsadores filtrada por software para ajustar de forma dinámica la posición de un servomotor en incrementos de $30^\circ$.

### Protocolo y Teoría Base
* **Señal PWM:** Configurada estrictamente a una frecuencia fija de $50\text{ Hz}$ (período de $20\text{ ms}$), tal como lo exigen los servomotores comerciales estándar (ej. SG90).
* **Ciclo de Trabajo y `CCR1`:** El ancho del pulso en estado alto ($3.3\text{ V}$) se regula mediante el registro `TIM4->CCR1`. El rango operativo seguro comprende desde $500\,\mu\text{s}$ ($0^\circ$) hasta $2500\,\mu\text{s}$ ($180^\circ$), abarcando una ventana útil de $2000\,\mu\text{s}$.
* **Desacoplamiento Hardware-Software:** El Timer 4 opera de manera autónoma por hardware. Las rutinas de software, como el retardo de antirrebote de los botones, se ejecutan en la CPU sin interrumpir ni alterar la generación continua de la onda PWM enviada al motor.
* **Fuente externa:** Se utilizo un porta pilas de 3 pilas AAA para lograr la corriente suficiente que necesita el servo motor.
---

## 2. Diagrama de Conexión de Hardware (Pinout)

El montaje físico interconecta la tarjeta de desarrollo STM32F407 con un servomotor de 3 hilos y dos pulsadores táctiles integrados.

```
+-------------------------------------------------------+
|                    Placa STM32F407                    |
|                                                       |
|   [PD12] ------------( Salida PWM )------------------> | Señal del Servo (Naranja/Amarillo)
|   [PE4 ] <-----------( Botón K0 )--------------------  | Pulsador Decremento (-30°)
|   [PE3 ] <-----------( Botón K1 )--------------------  | Pulsador Incremento (+30°)
|   [GND ] ------------( Tierra Común )----------------> | GND del Servo & Fuente Externa
|   [5V  ] ------------( Alimentación )----------------> | VCC del Servo (Rojo) [Fuente Externa]
+-------------------------------------------------------+
```

* **`PD12` (`TIM4_CH1`):** Configurado como Función Alternativa Push-Pull (AF2) para entregar la señal PWM.
* **`PE3` (`K1`) y `PE4` (`K0`):** Configurados como Entradas Digitales con resistencias de *Pull-up* internas habilitadas. Los botones cierran el circuito enviando un nivel bajo ($0\text{ V}$) al presionarse.

---

## 3. Guía de la API (Funciones de Alto Nivel y Control)

El código fuente está estructurado de manera modular con las siguientes rutinas de inicialización y control:

### `void PWM_GPIO_Init(void)`
* **Propósito:** Configura el pin 12 del puerto D (`PD12`) para la salida del temporizador por hardware.
* **Detalles de implementación:** Habilita el reloj de `GPIOD` en `AHB1ENR`, configura el registro `MODER` en Función Alternativa (`10`), establece salida Push-Pull (`OTYPER = 0`), velocidad alta (`OSPEEDR`), sin resistencias de polarización (`PUPDR = 00`), y mapea la función `AF2` en `AFR[1]` para el canal `TIM4_CH1`.

### `void PWM_TIM4_Init(void)`
* **Propósito:** Inicializa el Timer 4 como generador PWM exacto a $50\text{ Hz}$.
* **Detalles de implementación:** Habilita el reloj de `TIM4` en el bus `APB1`. Configura el Prescaler en `PSC = 15` ($16\text{ MHz} / 16 = 1\text{ MHz}$ de frecuencia de conteo, equivalente a $1\,\mu\text{s}$ por tick) y el Auto-Reload en `ARR = 19999` (período de $20\text{ ms}$). Configura el registro `CCMR1` en **Modo PWM 1** con precarga (`OC1PE`) activada, asigna un valor inicial de `CCR1 = 1500` ($90^\circ$), habilita la salida física en `CCER`, fuerza la actualización con `EGR`, y arranca el contador activando `CEN` en `CR1`.

### `void Buttons_Init(void)`
* **Propósito:** Inicializa los pulsadores K0 (`PE4`) y K1 (`PE3`).
* **Detalles de implementación:** Habilita el reloj del puerto `GPIOE`, configura los bits del registro `MODER` como entrada (`00`), y activa las resistencias internas de *Pull-up* mediante el registro `PUPDR` (`01`).

### `uint8_t Debounce_ReadButton(uint8_t pin)`
* **Propósito:** Filtro antirrebote por software y detector de flanco de bajada único.
* **Detalles de implementación:** Detecta el estado inicial bajo, ejecuta una pausa de $50\text{ ms}$ mediante `Delay_ms()`, reconfirma la estabilidad de la lectura y bloquea el flujo con un bucle `while` hasta que el usuario suelta el pulsador, garantizando una única transición válida por pulsación.

---

## 4. Instrucciones de Compilación y Estructura del Repositorio

## ⚙️ Instrucciones de Compilación y Ejecución

Para compilar y cargar este proyecto en la tarjeta STM32F407 de manera correcta, sigue estos pasos utilizando **STM32CubeIDE**:

1. **Requisitos previos:**
   - Tener instalado [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html) en tu computadora.
   - Tener a mano el cable de conexión USB (ST-Link) para la placa STM32F407.

2. **Importar el proyecto al entorno:**
   - Abre STM32CubeIDE.
   - En el menú superior, selecciona **File > Open Projects from File System...**.
   - Haz clic en *Directory*, busca y selecciona la carpeta raíz de este repositorio clonado en tu equipo, y presiona *Finish*.

3. **Compilar el código fuente:**
   - En el explorador de proyectos (Project Explorer), haz clic derecho sobre el nombre de tu proyecto.
   - Selecciona **Build Project** (o presiona el ícono del martillo 🔨 en la barra de herramientas superior).
   - Verifica en la consola inferior que la compilación finalice con éxito: **`0 Errors, 0 Warnings`**.

4. **Cargar el programa a la tarjeta (Flashing):**
   - Conecta físicamente tu placa STM32F407 al puerto USB de la computadora.
   - Haz clic en el botón **Run** (ícono de reproducción verde ▶) o en **Debug** (ícono de insecto 🐛) en la barra superior.
   - Si aparece una ventana de configuración de depuración (*Debug Configurations*), déjala por defecto con los parámetros del ST-Link y presiona *OK* / *Proceed*.

5. **Verificación en hardware:**
   - Una vez cargado el programa, presiona los botones integrados `K0` y `K1` (pines PE3 y PE4) para verificar el incremento y decremento de los escalones de $30^\circ$ en el servomotor conectado al pin `PD12`.

### Estructura de Carpetas del Proyecto:
```text
/
├── src/
│   └── main.c (Lógica completa bare-metal estructurada)
├── docs/
│   └── esquema_conexiones_y_capturas_osciloscopio.pdf
└── README.md
```

