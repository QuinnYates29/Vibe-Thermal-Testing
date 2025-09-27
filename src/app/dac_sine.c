#include "stm32g4xx_hal.h"
#include "dac_sine.h"
#include <math.h>

/* ===== Handles ===== */
static DAC_HandleTypeDef  hdac1;
static DMA_HandleTypeDef  hdma_dac1_ch2;
static TIM_HandleTypeDef  htim6;

/* ===== Sine LUT (12-bit, 0..4095) ===== */
#define LUT_LEN   256u
static uint16_t s_lut[LUT_LEN];

/* ===== State ===== */
static volatile float s_freq_hz = FREQ_MIN_HZ;   /* start at 20 Hz */

/* ===== Internal fns ===== */
static void LUT_Build(void);
static void GPIO_Init(void);
static void DMA_Init_DAC1_CH2(void);
static void DAC1_CH2_Init(void);
static void TIM6_Init(void);
static void Retune_TIM6(float f_hz);

/* ---------- Implementation ---------- */

static void LUT_Build(void)
{
  for (uint32_t i = 0; i < LUT_LEN; ++i) {
    float theta = (2.0f * (float)M_PI * (float)i) / (float)LUT_LEN;
    float s     = 0.5f + 0.5f * sinf(theta);           /* 0..1 */
    uint32_t v  = (uint32_t)lrintf(s * 4095.0f);       /* 12-bit */
    if (v > 4095u) v = 4095u;
    s_lut[i] = (uint16_t)v;
  }
}

static void GPIO_Init(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* PA5 as analog (DAC1_OUT2) */
  GPIO_InitTypeDef a = {0};
  a.Pin   = GPIO_PIN_5;
  a.Mode  = GPIO_MODE_ANALOG;
  a.Pull  = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &a);

  /* Optional: configure buttons here if used */
}

static void DMA_Init_DAC1_CH2(void)
{
  __HAL_RCC_DMAMUX1_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA1 Channel 4 is mapped to DAC1_CH2 */
  hdma_dac1_ch2.Instance                 = DMA1_Channel4;
  hdma_dac1_ch2.Init.Request             = DMA_REQUEST_DAC1_CHANNEL2;
  hdma_dac1_ch2.Init.Direction           = DMA_MEMORY_TO_PERIPH;
  hdma_dac1_ch2.Init.PeriphInc           = DMA_PINC_DISABLE;
  hdma_dac1_ch2.Init.MemInc              = DMA_MINC_ENABLE;
  hdma_dac1_ch2.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  hdma_dac1_ch2.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
  hdma_dac1_ch2.Init.Mode                = DMA_CIRCULAR;
  hdma_dac1_ch2.Init.Priority            = DMA_PRIORITY_HIGH;
  HAL_DMA_Init(&hdma_dac1_ch2);

  /* Link DMA to DAC channel 2 */
  __HAL_LINKDMA(&hdac1, DMA_Handle2, hdma_dac1_ch2);

  /* Enable DMA IRQ */
  HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
}

static void DAC1_CH2_Init(void)
{
  __HAL_RCC_DAC1_CLK_ENABLE();

  hdac1.Instance = DAC1;
  HAL_DAC_Init(&hdac1);

  DAC_ChannelConfTypeDef s = {0};
  s.DAC_Trigger      = DAC_TRIGGER_T6_TRGO;  /* TIM6 update triggers DAC */
  s.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  s.DAC_UserTrimming = DAC_TRIMMING_FACTORY;
  HAL_DAC_ConfigChannel(&hdac1, &s, DAC_CHANNEL_2);

  /* Start DAC with DMA feeding the LUT on channel 2 */
  HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_2,
                    (uint32_t*)s_lut, LUT_LEN, DAC_ALIGN_12B_R);
}

static void TIM6_Init(void)
{
  __HAL_RCC_TIM6_CLK_ENABLE();

  htim6.Instance = TIM6;
  htim6.Init.Prescaler           = 0;
  htim6.Init.CounterMode         = TIM_COUNTERMODE_UP;
  htim6.Init.Period              = 1000 - 1;
  htim6.Init.AutoReloadPreload   = TIM_AUTORELOAD_PRELOAD_DISABLE;
  HAL_TIM_Base_Init(&htim6);

  TIM_MasterConfigTypeDef m = {0};
  m.MasterOutputTrigger = TIM_TRGO_UPDATE;
  m.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
  HAL_TIMEx_MasterConfigSynchronization(&htim6, &m);

  HAL_TIM_Base_Start(&htim6);
}

/* f_out = timer_clock / (ARR+1) / LUT_LEN  =>  ARR = timer_clock/(f_out*LUT_LEN) - 1 */
static void Retune_TIM6(float f_hz)
{
  uint32_t timclk = HAL_RCC_GetPCLK1Freq();

  RCC_ClkInitTypeDef ci; uint32_t flashLatency;
  HAL_RCC_GetClockConfig(&ci, &flashLatency);
  if (ci.APB1CLKDivider != RCC_HCLK_DIV1) timclk *= 2u;

  float fs = f_hz * (float)LUT_LEN;
  float arr_f = ((float)timclk / fs) - 1.0f;

  if (arr_f < 1.0f)    arr_f = 1.0f;
  if (arr_f > 65535.f) arr_f = 65535.f;

  uint32_t arr = (uint32_t)lrintf(arr_f);

  __HAL_TIM_DISABLE(&htim6);
  __HAL_TIM_SET_AUTORELOAD(&htim6, arr);
  __HAL_TIM_SET_COUNTER(&htim6, 0);
  __HAL_TIM_ENABLE(&htim6);
}

/* ---------- Public API ---------- */

void SineGen_Init(void)
{
  HAL_Init();

  LUT_Build();
  GPIO_Init();
  DMA_Init_DAC1_CH2();
  DAC1_CH2_Init();
  TIM6_Init();
  Retune_TIM6(s_freq_hz);      /* start at 20 Hz */
}

float SineGen_GetFreqHz(void) { return s_freq_hz; }

void SineGen_SetFreqHz(float f_hz)
{
  if (f_hz < FREQ_MIN_HZ) f_hz = FREQ_MIN_HZ;
  if (f_hz > FREQ_MAX_HZ) f_hz = FREQ_MAX_HZ;
  s_freq_hz = f_hz;
  Retune_TIM6(s_freq_hz);
}

void SineGen_Task(void)
{
  /* If you have buttons hooked up, poll them here and call SineGen_SetFreqHz() */
}

/* DMA IRQ for DAC1_CH2 */
void DMA1_Channel4_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_dac1_ch2);
}