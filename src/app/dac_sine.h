#pragma once
#include "stm32g4xx_hal.h"

/* Public API */
void SineGen_Init(void);
void SineGen_Task(void);

/* Frequency range from your potentiometer (Hz) */
#define SINEGEN_FMIN_HZ  0.0f
#define SINEGEN_FMAX_HZ  50.0f

/* Number of samples per period (smoother at low Hz if larger) */
#define SINEGEN_SINE_LEN 1024

/* Timer input clock (Hz). If using HSI 16 MHz and APB prescalers=1, TIM6=16 MHz. */
#define SINEGEN_TIM_CLK_HZ  16000000UL

