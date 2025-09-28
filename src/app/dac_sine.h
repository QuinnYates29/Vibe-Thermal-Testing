#pragma once
#include "stm32g4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========= Build-time toggle =========
   1 = output on PA5 (DAC1_CH2)   [often tied to user LED on Nucleo]
   0 = output on PA4 (DAC1_CH1)   [cleaner on many boards]
*/
#define SINE_OUT_ON_PA5   1

/* ========= Frequency settings ========= */
#define SINEGEN_F_MIN     20u
#define SINEGEN_F_MAX     100u
#define SINEGEN_F_STEP    10u

/* ========= Sine LUT settings ========= */
#define SINEGEN_SINE_LEN  256u

/* ========= Public API ========= */
void SineGen_Init(void);
void SineGen_Task(void);

/* ========= Error handler (weak) ========= */
void Error_Handler(void);

#ifdef __cplusplus
}
#endif
