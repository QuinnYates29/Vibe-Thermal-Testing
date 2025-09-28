#include "stm32g4xx_hal.h"
#include "dac_sine.h"
#include <math.h>

#define LUT_LEN 64

static DAC_HandleTypeDef hdac1;
static uint16_t s_lut[LUT_LEN];

static void GPIO_Init_PA5(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIO_InitTypeDef a = {0};
  a.Pin   = GPIO_PIN_5;           // PA5 = DAC1_OUT2
  a.Mode  = GPIO_MODE_ANALOG;
  a.Pull  = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &a);
}

static void DAC1_CH2_Init(void)
{
  __HAL_RCC_DAC1_CLK_ENABLE();

  hdac1.Instance = DAC1;
  HAL_DAC_Init(&hdac1);

  DAC_ChannelConfTypeDef s = {0};
  s.DAC_Trigger = DAC_TRIGGER_NONE;          // software update
  s.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  HAL_DAC_ConfigChannel(&hdac1, &s, DAC_CHANNEL_2);

  HAL_DAC_Start(&hdac1, DAC_CHANNEL_2);
}

static void LUT_Build(void)
{
  for (uint32_t i = 0; i < LUT_LEN; ++i)
  {
    float phase = (2.0f * 3.14159265f * i) / LUT_LEN;
    float s = 0.5f + 0.49f * sinf(phase);   // 0..~0.98
    s_lut[i] = (uint16_t)(s * 4095.0f);     // 12-bit
  }
}

void SineGen_Init(void)
{
  HAL_Init();                // in case not called elsewhere
  // Minimal clock (assumes system_stm32g4xx.c already sets clocks)
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();

  GPIO_Init_PA5();
  DAC1_CH2_Init();
  LUT_Build();
}

void SineGen_Task(void)
{
  // Walk the table slowly: ~16 Hz with 1 ms per sample and 64 samples
  static uint32_t idx = 0;

  HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, s_lut[idx]);
  idx = (idx + 1) % LUT_LEN;

  HAL_Delay(1);  // 1 ms/sample → ~15.6 Hz. Change to HAL_Delay(0) for faster.
}
