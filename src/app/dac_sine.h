#pragma once
#include "stm32g4xx_hal.h"
#include <stdint.h>

/* ===== USER CONFIG: button pins =====
   Active-low with pull-ups, change here if wiring changes */
#define BTN_UP_GPIO_PORT    GPIOA
#define BTN_UP_PIN          GPIO_PIN_2   /* UP button */

#define BTN_DN_GPIO_PORT    GPIOA
#define BTN_DN_PIN          GPIO_PIN_3   /* DOWN button */

/* Frequency window and step (Hz) */
#define FREQ_MIN_HZ         20.0f
#define FREQ_MAX_HZ         100.0f
#define FREQ_STEP_HZ        10.0f

void  SineGen_Init(void);
void  SineGen_Task(void);          /* polls buttons & retunes */
float SineGen_GetFreqHz(void);
void  SineGen_SetFreqHz(float f_hz);
