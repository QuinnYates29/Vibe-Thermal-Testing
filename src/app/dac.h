#include <stdint.h>
#include <stdbool.h>
#include <stm32g4xx_hal.h>

#ifndef core_DAC_H 
#define core_DAC_H

//bitmasks
#define CORE_DAC_ALLOWED_DAC1       0x0001
#define CORE_DAC_ALLOWED_OPAMP1     0x0100
#define CORE_DAC_ALLOWED_OPAMP2     0x0200
#define CORE_DAC_ALLOWED_OPAMP3     0x0400
#define CORE_DAC_ALLOWED_OPAMP4     0x0800
#define CORE_DAC_ALLOWED_OPAMP5     0x1000
#define CORE_DAC_ALLOWED_OPAMP6     0x2000
#define CORE_DAC_ALLOWED_DAC_MASK   0x0001
#define CORE_DAC_ALLOWED_OPAMP_MASK 0x3F00

//Channel Identification
//STM32 : PA4 = DAC1_CHANNEL1 PA4 = DAC1_CHANNEL2
#define CORE_DAC_CHANNEL1    1u   /* PA4  -> DAC1_CH1 */
#define CORE_DAC_CHANNEL2    2u   /* PA5  -> DAC1_CH2 */

//sine defualts, can be overriden
#ifndef CORE_DAC_SINE_FS_HZ
#define CORE_DAC_SINE_FS_HZ         (25000u)    /* fixed sample rate (Hz) */
#endif

#ifndef CORE_DAC_SINE_LUT_LEN
#define CORE_DAC_SINE_LUT_LEN       (512u)      /* power of 2 recommended */
#endif

#ifndef CORE_DAC_SINE_CHUNK
#define CORE_DAC_SINE_CHUNK         (256u)      /* half-buffer size for DMA double-buffering */
#endif

#ifndef CORE_DAC_F_MIN_HZ
#define CORE_DAC_F_MIN_HZ           (1.0f)
#endif

#ifndef CORE_DAC_F_MAX_HZ
#define CORE_DAC_F_MAX_HZ           (2000.0f)
#endif

//Driver Structures
typedef struct core_DAC_def_s {
    /* -------- Constant / board-level fields (filled once by app/board code) -------- */
    GPIO_TypeDef *port;             /* GPIO port for the DAC pin (e.g., GPIOA)      */
    uint32_t      pin;              /* GPIO pin (e.g., GPIO_PIN_4 or GPIO_PIN_5)    */
    uint16_t      allowed_connections; /* bitmask: CORE_DAC_ALLOWED_*                */
    uint8_t       channel;          /* CORE_DAC_CH1 (PA4) or CORE_DAC_CH2 (PA5)

     /* Trigger timer and clocking (fixed-fs NCO) */
    TIM_TypeDef  *trigger_tim;      /* usually TIM6                                  */
    uint32_t      tim_input_hz;     /* clock into trigger_tim after prescalers       */

    /* Sine/NCO defaults (can be changed later by setters) */
    uint32_t      fs_hz;            /* sample rate (default CORE_DAC_SINE_FS_HZ)     */
    uint16_t      lut_len;          /* LUT length (default CORE_DAC_SINE_LUT_LEN)    */

    /* -------- Runtime / edited by the program -------- */
    DAC_TypeDef  *dac;              /* DAC instance (DAC1 on STM32G4)                */
    bool          running;          /* streaming active state                         */
} core_DAC_def_t;

//Core DAC API
/** Initialize the DAC peripheral (clocks, base registers). */
bool core_DAC_init(DAC_TypeDef *dac);

/** Configure a DAC pin (GPIO analog mode) and select a logical DAC channel. */
bool core_DAC_setup_pin(GPIO_TypeDef *port, uint32_t pin, uint8_t channel);

/** Write a single 12-bit right-aligned sample directly to the selected channel. */
bool core_DAC_write_u12(uint8_t channel, uint16_t value_0_4095);

//NCO(Numerically Controlled Oscillator) engine
//TIM->TRGO->DAC with DMA+NCO
bool core_DAC_sine_init(core_DAC_def_t *cfg);

//start continous sine generation
/**
freq_hz is between values [CORE_DAC_F_MIN_HZ, CORE_DAC_F_MAX_HZ]
 */
bool core_DAC_sine_start(float freq_hz, float amplitude_0to1, float offset_0to1);

//sine gen stop
void core_DAC_sine_stop(void);

//runtime controls ie)Setters & Getters
void  core_DAC_sine_set_frequency(float freq_hz);
void  core_DAC_sine_set_amp_offset(float amplitude_0to1, float offset_0to1);
bool  core_DAC_sine_is_running(void);
float core_DAC_sine_get_frequency(void);
uint32_t core_DAC_sine_get_fs(void);
uint8_t  core_DAC_sine_get_channel(void);

#endif /* CORE_DAC_H */

