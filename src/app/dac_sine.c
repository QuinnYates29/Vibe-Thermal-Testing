#include "dac_sine.h"
#include <math.h>

/* ===== Peripheral handles ===== */
static ADC_HandleTypeDef  hadc1;
static DAC_HandleTypeDef  hdac1;
static DMA_HandleTypeDef  hdma_dac1_ch1;
static TIM_HandleTypeDef  htim6;

/* ===== Sine LUT ===== */
static uint16_t sine_lut[SINEGEN_SINE_LEN];

/* ===== Forward decls ===== */
static void SystemClock_SimpleHSI16(void);
static void GPIO_Init_PA0_PA4(void);
static void DMA_Init_DAC1_CH1(void);
static void DAC1_CH1_Init_TIM6_Trigger(void);
static void TIM6_Init_with_fs(uint32_t fs);
static void ADC1_Init_PA0(void);
static float map_adc_to_freq(uint16_t raw);
static void set_sample_rate(float fs);
static void build_sine_lut(void);

/* ===== Public ===== */
void SineGen_Init(void)
{
  HAL_Init();
  SystemClock_SimpleHSI16();   /* simple HSI=16MHz clock */

  GPIO_Init_PA0_PA4();
  DMA_Init_DAC1_CH1();
  DAC1_CH1_Init_TIM6_Trigger();
  ADC1_Init_PA0();

  build_sine_lut();

  HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
  HAL_ADC_Start(&hadc1);

  HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1,
                    (uint32_t*)sine_lut, SINEGEN_SINE_LEN,
                    DAC_ALIGN_12B_R);

  float f_out = (SINEGEN_FMIN_HZ + SINEGEN_FMAX_HZ) * 0.5f;
  float fs = f_out * (float)SINEGEN_SINE_LEN;

  TIM6_Init_with_fs((uint32_t)fs);
  HAL_TIM_Base_Start(&htim6);
}

void SineGen_Task(void)
{
  if (HAL_ADC_PollForConversion(&hadc1, 1) == HAL_OK) {
    uint16_t raw = HAL_ADC_GetValue(&hadc1);     /* 0..4095 */
    float f_out  = map_adc_to_freq(raw);         /* 0..50 Hz */
    float fs     = f_out * (float)SINEGEN_SINE_LEN;
    set_sample_rate(fs);
  }
}

/* ===== Clock: HSI 16 MHz ===== */
static void SystemClock_SimpleHSI16(void)
{
  RCC_OscInitTypeDef osc = {0};
  RCC_ClkInitTypeDef clk = {0};

  osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  osc.HSIState       = RCC_HSI_ON;
  osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  osc.PLL.PLLState   = RCC_PLL_NONE;
  HAL_RCC_OscConfig(&osc);

  clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  clk.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
  clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  clk.APB1CLKDivider = RCC_HCLK_DIV1;
  clk.APB2CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0);

  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
}

/* ===== GPIO: PA4 DAC out, PA0 ADC in ===== */
static void GPIO_Init_PA0_PA4(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitTypeDef g = {0};
  g.Mode = GPIO_MODE_ANALOG;
  g.Pull = GPIO_NOPULL;

  g.Pin = GPIO_PIN_4; /* PA4 -> DAC1_OUT1 */
  HAL_GPIO_Init(GPIOA, &g);

 g.Pin = GPIO_PIN_0; /* PA0 -> ADC1_IN1 */
  HAL_GPIO_Init(GPIOA, &g);
}

/* ===== DMA for DAC1_CH1 ===== */
static void DMA_Init_DAC1_CH1(void)
{
  __HAL_RCC_DMAMUX1_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  hdma_dac1_ch1.Instance                 = DMA1_Channel3;
  /* DMAMUX request for DAC1 CH1: use the symbol your HAL defines */
  hdma_dac1_ch1.Init.Request 		 = DMA_REQUEST_DAC1_CHANNEL1;
  hdma_dac1_ch1.Init.Direction           = DMA_MEMORY_TO_PERIPH;
  hdma_dac1_ch1.Init.PeriphInc           = DMA_PINC_DISABLE;
  hdma_dac1_ch1.Init.MemInc              = DMA_MINC_ENABLE;
  hdma_dac1_ch1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  hdma_dac1_ch1.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
  hdma_dac1_ch1.Init.Mode                = DMA_CIRCULAR;
  hdma_dac1_ch1.Init.Priority            = DMA_PRIORITY_HIGH;
  HAL_DMA_Init(&hdma_dac1_ch1);

  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
}

/* ===== DAC1 CH1 with TIM6 trigger ===== */
static void DAC1_CH1_Init_TIM6_Trigger(void)
{
  __HAL_RCC_DAC1_CLK_ENABLE();

  hdac1.Instance = DAC1;
  HAL_DAC_Init(&hdac1);

  __HAL_LINKDMA(&hdac1, DMA_Handle1, hdma_dac1_ch1);

  DAC_ChannelConfTypeDef c = {0};
  c.DAC_Trigger                 = DAC_TRIGGER_T6_TRGO;
  c.DAC_OutputBuffer            = DAC_OUTPUTBUFFER_ENABLE;
  c.DAC_SampleAndHold           = DAC_SAMPLEANDHOLD_DISABLE;
  c.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_DISABLE;
  c.DAC_UserTrimming            = DAC_TRIMMING_FACTORY;
  HAL_DAC_ConfigChannel(&hdac1, &c, DAC_CHANNEL_1);
}

/* ===== TIM6: sample clock (TRGO on update) ===== */
static void TIM6_Init_with_fs(uint32_t fs)
{
  __HAL_RCC_TIM6_CLK_ENABLE();

  uint32_t psc    = 99; /* /100 */
  uint32_t timclk = SINEGEN_TIM_CLK_HZ / (psc + 1U);
  uint32_t arr    = (fs > 0) ? (timclk / fs) : 2000U;
  if (arr < 2) arr = 2;

  htim6.Instance = TIM6;
  htim6.Init.Prescaler         = psc;
  htim6.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim6.Init.Period            = arr - 1U;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  HAL_TIM_Base_Init(&htim6);

  TIM_MasterConfigTypeDef m = {0};
  m.MasterOutputTrigger = TIM_TRGO_UPDATE;
  m.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
  HAL_TIMEx_MasterConfigSynchronization(&htim6, &m);
}

/* ===== ADC1 on PA0 ===== */
static void ADC1_Init_PA0(void)
{
  __HAL_RCC_ADC12_CLK_ENABLE();

  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler        = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait      = DISABLE;
  hadc1.Init.ContinuousConvMode    = ENABLE;
  hadc1.Init.NbrOfConversion       = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;
  HAL_ADC_Init(&hadc1);

  ADC_ChannelConfTypeDef s = {0};
  s.Channel      = ADC_CHANNEL_1;               /* PA0 */
  s.Rank         = ADC_REGULAR_RANK_1;
  /* Pick any valid sampling-time macro your HAL defines */
  #if defined(ADC_SAMPLETIME_19CYCLES_5)
  s.SamplingTime = ADC_SAMPLETIME_19CYCLES_5;
  #elif defined(ADC_SAMPLETIME_12CYCLES_5)
  s.SamplingTime = ADC_SAMPLETIME_12CYCLES_5;
  #elif defined(ADC_SAMPLETIME_24CYCLES_5)
  s.SamplingTime = ADC_SAMPLETIME_24CYCLES_5;
  #elif defined(ADC_SAMPLETIME_47CYCLES_5)
  s.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
  #else
  s.SamplingTime = ADC_SAMPLETIME_2CYCLES_5; /* safe default present in all packs */
  #endif


  HAL_ADC_ConfigChannel(&hadc1, &s);
}

/* ===== Map pot (0..4095) → frequency (0..50 Hz) ===== */
static float map_adc_to_freq(uint16_t raw)
{
  float t = (float)raw / 4095.0f;
  return SINEGEN_FMIN_HZ + t * (SINEGEN_FMAX_HZ - SINEGEN_FMIN_HZ);
}

/* ===== Adjust TIM6 rate ===== */
static void set_sample_rate(float fs)
{
  if (fs < 0.1f) fs = 0.1f;

  const uint16_t pscs[] = {0,1,9,49,99,199,399,999};
  uint32_t best_psc = 99, best_arr = 2000;

  for (size_t i = 0; i < sizeof(pscs)/sizeof(pscs[0]); ++i) {
    uint32_t clk = SINEGEN_TIM_CLK_HZ / (pscs[i] + 1U);
    uint32_t arr = (uint32_t)((float)clk / fs);
    if (arr > 1 && arr <= 65535) { best_psc = pscs[i]; best_arr = arr; break; }
  }

  __HAL_TIM_DISABLE(&htim6);
  __HAL_TIM_SET_PRESCALER(&htim6, best_psc);
  __HAL_TIM_SET_AUTORELOAD(&htim6, best_arr - 1U);
  __HAL_TIM_SET_COUNTER(&htim6, 0);
  __HAL_TIM_ENABLE(&htim6);
}

/* ===== Build LUT (12-bit unsigned) ===== */
static void build_sine_lut(void)
{
  const float two_pi = 6.283185307179586f;
  for (uint32_t n = 0; n < SINEGEN_SINE_LEN; ++n) {
    float phase = two_pi * ((float)n / (float)SINEGEN_SINE_LEN);
    float s     = sinf(phase);                 /* -1 .. +1 */
    float v     = 2048.0f + 2047.0f * s;       /* 0 .. 4095 */
    if (v < 0.0f)    v = 0.0f;
    if (v > 4095.0f) v = 4095.0f;
    sine_lut[n] = (uint16_t)(v + 0.5f);
  }
}

/* ===== DMA IRQ (optional callbacks) ===== */
void DMA1_Channel3_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_dac1_ch1);
}

