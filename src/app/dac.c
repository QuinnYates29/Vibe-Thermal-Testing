#include "dac.h"
#include "core_config.h"
#include <math.h>
#include <string.h>

/** 
*DMA channel selection
*   Channel3 common for pa4 channel5 common for pa5
*/
#ifndef CORE_DAC_DMA_INSTANCE
#define CORE_DAC_DMA_INSTANCE           DMA1
#endif
#ifndef CORE_DAC_DMA_CHANNEL_CHANNEL1
#define CORE_DAC_DMA_CHANNEL_CHANNEL1        DMA1_Channel3
#endif
#ifndef CORE_DAC_DMA_CHANNEL_CHANNEL2
#define CORE_DAC_DMA_CHANNEL_CHANNEL2        DMA1_Channel4
#endif

/**
*DMAMUX IDs
*needs to match HAL version meaning same channel as above DMA
*/
#ifndef CORE_DAC_DMAMUX_REQ_CHANNEL1
  #if defined(DMA_REQUEST_DAC1_CHANNEL1)
    #define CORE_DAC_DMAMUX_REQ_CHANNEL1     DMA_REQUEST_DAC1_CHANNEL1
  #elif defined(LL_DMAMUX_REQ_DAC1_CHCANNEL1)
    #define CORE_DAC_DMAMUX_REQ_CHANNEL1     LL_DMAMUX_REQ_DAC1_CHANNEL1
  #else
    #error "Define CORE_DAC_DMAMUX_REQ_CH1 to your DMAMUX request for DAC1_CH1."
  #endif
#endif

#ifndef CORE_DAC_DMAMUX_REQ_CHANNEL2
  #if defined(DMA_REQUEST_DAC1_CHANNEL2)
    #define CORE_DAC_DMAMUX_REQ_CHANNEL2     DMA_REQUEST_DAC1_CHANNEL2
  #elif defined(LL_DMAMUX_REQ_DAC1_CHANNEL2)
    #define CORE_DAC_DMAMUX_REQ_CHANNEL2     LL_DMAMUX_REQ_DAC1_CHANNEL2
  #else
    #error "Define CORE_DAC_DMAMUX_REQ_CH2 to your DMAMUX request for DAC1_CH2."
  #endif
#endif

/**
*NVIC IRQ for chosen DMA 
*Controls Interupts
*/
#ifndef CORE_DAC_DMA_IRQn_CHANNEL1
#define CORE_DAC_DMA_IRQn_CHANNEL1           DMA1_Channel3_IRQn
#endif
#ifndef CORE_DAC_DMA_IRQn_CHANNEL2
#define CORE_DAC_DMA_IRQn_CHANNEL2           DMA1_Channel4_IRQn
#endif

/**
Static State
*/
static core_DAC_def_t g_cfg;                  /* user config snapshot */
static DAC_HandleTypeDef   g_hdac;
static TIM_HandleTypeDef   g_htim6;
static DMA_HandleTypeDef   g_hdma;

static volatile bool       g_running = false;

/**
*LUT and double buffer work area
*use double buffer for reliability pruposes
*will decrease the chance of having buffer when trying to switch data output
*/
#ifndef CORE_DAC_SINE_LUT_LEN
#define CORE_DAC_SINE_LUT_LEN 512u
#endif
#ifndef CORE_DAC_SINE_CHUNK
#define CORE_DAC_SINE_CHUNK   256u
#endif
static uint16_t            g_lut[CORE_DAC_SINE_LUT_LEN];
static uint16_t            g_buf[CORE_DAC_SINE_CHUNK * 2];  /* double-buffer */

/**
NCO
uses phase accumulator 
*/
static volatile uint32_t   g_phase = 0;
static volatile uint32_t   g_phase_inc = 0; 

/* Runtime params */
static volatile float      g_freq_hz = 50.0f;
static volatile uint16_t   g_amp_q12 = 2047;  /* 0..4095 scale on +/-2047 centered sine */
static volatile uint16_t   g_off_q12 = 2048;  /* 0..4095 absolute offset */

/**
*helper to index bit leangth
*/
static uint8_t lut_index_bits(void) {
    uint16_t n = g_cfg.lut_len ? g_cfg.lut_len : CORE_DAC_SINE_LUT_LEN;
    uint8_t bits = 0;
    while ((1u << bits) < n && bits < 16) bits++;
    return bits;
}

/**
*Clamp helper
*/
static inline uint16_t clamp_u12(int32_t x) {
    if (x < 0) return 0;
    if (x > 4095) return 4095;
    return (uint16_t)x;
}


/* ===========================================================
 *                   LOW-LEVEL SETUP HELPERS
 * ===========================================================
 */
static void build_lut(uint16_t len) {
    const double two_pi = 6.28318530717958647692;
    for (uint16_t i = 0; i < len; ++i) {
        double theta = two_pi * (double)i / (double)len;
        /* Map sin(theta) from [-1,1] to [0..4095] with center ~2048, amplitude ~2047 */
        int32_t v = (int32_t)lrint(2048.0 + 2047.0 * sin(theta));
        if (v < 0) v = 0; 
        if (v > 4095) v = 4095;
        g_lut[i] = (uint16_t)v;
    }
}

/**
*compute new value to pass into lut table to get wanted frequency
*
*/
static uint32_t compute_phase_inc(float freq_hz, uint32_t fs_hz) {
    if (freq_hz < CORE_DAC_F_MIN_HZ) freq_hz = CORE_DAC_F_MIN_HZ;
    if (freq_hz > CORE_DAC_F_MAX_HZ) freq_hz = CORE_DAC_F_MAX_HZ;
    double ratio = (double)freq_hz / (double)fs_hz;
    uint32_t inc = (uint32_t)llround(ratio * 4294967296.0); /* 2^32 */
    if (inc == 0) inc = 1;
    return inc;
}

/* Derive TIM6 input clock. Prefer cfg->tim_input_hz if provided; else infer. */
static uint32_t get_tim6_clk_hz(void) {
    if (g_cfg.tim_input_hz) return g_cfg.tim_input_hz;

    /* PCLK1 as base */
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    /* If APB1 prescaler != 1, timer clock is PCLK1 * 2 on STM32 */
    uint32_t cfgr = RCC->CFGR;
    uint32_t ppre1 = (cfgr & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
    bool ppre1_div1 = (ppre1 == 0b000);
    return ppre1_div1 ? pclk1 : (pclk1 * 2u);
}

static HAL_StatusTypeDef tim6_init_fixed_fs(uint32_t fs_hz) {
    __HAL_RCC_TIM6_CLK_ENABLE();

    /* Strategy: aim for ~1 MHz timer tick for easy ARR math, within 16-bit PSC */
    uint32_t timclk = get_tim6_clk_hz();
    uint32_t tick_hz = 1000000u;

    uint32_t psc = (timclk + tick_hz/2u) / tick_hz; /* round */
    if (psc == 0) psc = 1;
    psc -= 1;

    /* recompute actual tick */
    uint32_t actual_tick = timclk / (psc + 1u);

    uint32_t arr = (actual_tick + fs_hz/2u) / fs_hz; /* round */
    if (arr < 2u) arr = 2u; /* keep ARR >= 2 for stable TRGO rate */
    arr -= 1;

    g_htim6.Instance = TIM6;
    g_htim6.Init.Prescaler = (uint16_t)psc;
    g_htim6.Init.Period    = (uint16_t)arr;
    g_htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
    g_htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_Base_Init(&g_htim6) != HAL_OK) return HAL_ERROR;

    TIM_MasterConfigTypeDef mcfg = {0};
    mcfg.MasterOutputTrigger = TIM_TRGO_UPDATE;
    mcfg.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&g_htim6, &mcfg) != HAL_OK) return HAL_ERROR;

    return HAL_OK;
}

static HAL_StatusTypeDef dac_channel_init(uint8_t channel) {
    __HAL_RCC_DAC1_CLK_ENABLE();

    g_hdac.Instance = DAC1;
    if (HAL_DAC_Init(&g_hdac) != HAL_OK) return HAL_ERROR;

    DAC_ChannelConfTypeDef cfg = {0};
    cfg.DAC_Trigger      = DAC_TRIGGER_T6_TRGO;
    cfg.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;

    uint32_t ch = (channel == CORE_DAC_CHANNEL2) ? DAC_CHANNEL_2 : DAC_CHANNEL_1;
    if (HAL_DAC_ConfigChannel(&g_hdac, &cfg, ch) != HAL_OK) return HAL_ERROR;

    return HAL_OK;
}

/* Select DMA channel and DMAMUX request for chosen DAC channel */
static void dma_pick_channel_and_request(uint8_t channel,
                                         DMA_Channel_TypeDef **dma_ch,
                                         uint32_t *request_id,
                                         IRQn_Type *irqn)
{
    if (channel == CORE_DAC_CHANNEL2) {
        *dma_ch   = CORE_DAC_DMA_CHANNEL_CHANNEL2;
        *request_id = CORE_DAC_DMAMUX_REQ_CHANNEL2;
        *irqn = CORE_DAC_DMA_IRQn_CHANNEL2;
    } else {
        *dma_ch   = CORE_DAC_DMA_CHANNEL_CHANNEL1;
        *request_id = CORE_DAC_DMAMUX_REQ_CHANNEL1;
        *irqn = CORE_DAC_DMA_IRQn_CHANNEL1;
    }
}

static HAL_StatusTypeDef dma_init_for_dac(uint8_t channel) {
    __HAL_RCC_DMA1_CLK_ENABLE();  /* DMA1 on G4 */
    __HAL_RCC_DMAMUX1_CLK_ENABLE();

    DMA_Channel_TypeDef *dma_ch;
    uint32_t req_id;
    IRQn_Type irqn;
    dma_pick_channel_and_request(channel, &dma_ch, &req_id, &irqn);

    /* Deinit first for safety */
    g_hdma.Instance = dma_ch;
    HAL_DMA_DeInit(&g_hdma);

    g_hdma.Init.Request             = req_id;
    g_hdma.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    g_hdma.Init.PeriphInc           = DMA_PINC_DISABLE;
    g_hdma.Init.MemInc              = DMA_MINC_ENABLE;
    g_hdma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD; /* 12-bit right fits in 16 bits */
    g_hdma.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
    g_hdma.Init.Mode                = DMA_CIRCULAR;
    g_hdma.Init.Priority            = DMA_PRIORITY_HIGH;

    if (HAL_DMA_Init(&g_hdma) != HAL_OK) return HAL_ERROR;

    /* Link DMA to DAC handle (Handle1 for CH1, Handle2 for CH2 per HAL) */
    if (channel == CORE_DAC_CHANNEL2) {
        __HAL_LINKDMA(&g_hdac, DMA_Handle2, g_hdma);
    } else {
        __HAL_LINKDMA(&g_hdac, DMA_Handle1, g_hdma);
    }

    /* Enable NVIC for this DMA channel */
    HAL_NVIC_SetPriority(irqn, 5, 0);
    HAL_NVIC_EnableIRQ(irqn);

    return HAL_OK;
}

/**
*fill buffer
*/
static void nco_fill(uint16_t *dst, uint16_t count) {
    const uint16_t *lut = g_lut;
    const uint16_t len  = (g_cfg.lut_len ? g_cfg.lut_len : CORE_DAC_SINE_LUT_LEN);
    const uint8_t  idx_bits = lut_index_bits();
    const uint8_t  idx_shift = 32 - idx_bits;
    const int32_t  center = 2048;

    /* Snapshot scales atomically (single 32-bit reads on Cortex-M are atomic) */
    uint32_t phase_inc = g_phase_inc;
    uint16_t amp_q12   = g_amp_q12;
    uint16_t off_q12   = g_off_q12;

    for (uint16_t i = 0; i < count; ++i) {
        g_phase += phase_inc;
        uint32_t idx = (g_phase >> idx_shift);
        if (idx >= len) idx &= (len - 1u); /* LUT length should be power of two */
        int32_t centered = (int32_t)lut[idx] - center;  /* +/-2047 approx */

        /* Scale centered value by amplitude, then add offset */
        /* amp_q12 maps desired amplitude on +/-2047 range; divide by 2047 to normalize */
        int32_t scaled = (centered * (int32_t)amp_q12) / 2047;
        int32_t sample = (int32_t)off_q12 + scaled;

        dst[i] = clamp_u12(sample);
    }
}

/**
*Core DAC api implementations
*/
bool core_DAC_init(DAC_TypeDef *dac) {
    (void)dac; /* Only DAC1 exists on G4 family; keep signature symmetric with ADC API */
    __HAL_RCC_DAC1_CLK_ENABLE();
    g_hdac.Instance = DAC1;
    return (HAL_DAC_Init(&g_hdac) == HAL_OK);
}

bool core_DAC_setup_pin(GPIO_TypeDef *port, uint32_t pin, uint8_t channel) {
    /* Basic GPIO analog setup, no pulls */
    if (!port) return false;
    if (channel != CORE_DAC_CHANNEL1 && channel != CORE_DAC_CHANNEL2) return false;

    /* Enable GPIO clock heuristically; user can pre-enable elsewhere too */
    if (port == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
    else if (port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    else if (port == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
    else if (port == GPIOD) __HAL_RCC_GPIOD_CLK_ENABLE();
    else if (port == GPIOE) __HAL_RCC_GPIOE_CLK_ENABLE();

    GPIO_InitTypeDef gi = {0};
    gi.Pin   = pin;
    gi.Mode  = GPIO_MODE_ANALOG;
    gi.Pull  = GPIO_NOPULL;
    gi.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &gi);

    return true;
}

bool core_DAC_write_u12(uint8_t channel, uint16_t value_0_4095) {
    if (g_running) return false; /* don’t interfere while streaming */
    uint32_t ch = (channel == CORE_DAC_CHANNEL2) ? DAC_CHANNEL_2 : DAC_CHANNEL_1;
    if (HAL_DAC_ConfigChannel(&g_hdac, &(DAC_ChannelConfTypeDef){
        .DAC_Trigger = DAC_TRIGGER_NONE,
        .DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE
    }, ch) != HAL_OK) return false;

    if (HAL_DAC_SetValue(&g_hdac, ch, DAC_ALIGN_12B_R, value_0_4095) != HAL_OK) return false;
    if (HAL_DAC_Start(&g_hdac, ch) != HAL_OK) return false;
    return true;
}

/**
Sine NCO driver api
*/
bool core_DAC_sine_init(core_DAC_def_t *cfg) {
    if (!cfg) return false;

    /* Snapshot immutable config (with sane defaults) */
    memset(&g_cfg, 0, sizeof(g_cfg));
    g_cfg = *cfg;
    if (g_cfg.fs_hz == 0)     g_cfg.fs_hz = CORE_DAC_SINE_FS_HZ;
    if (g_cfg.lut_len == 0)   g_cfg.lut_len = CORE_DAC_SINE_LUT_LEN;
    if (g_cfg.channel != CORE_DAC_CHANNEL1 && g_cfg.channel != CORE_DAC_CHANNEL2)
        g_cfg.channel = CORE_DAC_CHANNEL1;

    /* GPIO to analog */
    if (!core_DAC_setup_pin(g_cfg.port, g_cfg.pin, g_cfg.channel)) return false;

    /* TIM6 @ fs_hz with TRGO=Update */
    if (tim6_init_fixed_fs(g_cfg.fs_hz) != HAL_OK) return false;

    /**DAC channel with trigger = TIM6 TRGO */
    if (dac_channel_init(g_cfg.channel) != HAL_OK) return false;

    /* DMA / DMAMUX configured and linked to DAC */
    if (dma_init_for_dac(g_cfg.channel) != HAL_OK) return false;

    /* Build LUT once */
    build_lut(g_cfg.lut_len);

    /* Default amplitude/offset (centered, near full-scale) */
    g_amp_q12 = 2047;
    g_off_q12 = 2048;

    /* Default frequency (kept until user calls start) */
    g_freq_hz = 50.0f;
    g_phase = 0;
    g_phase_inc = compute_phase_inc(g_freq_hz, g_cfg.fs_hz);

    return true;
}

bool core_DAC_sine_start(float freq_hz, float amplitude_0to1, float offset_0to1) {
    if (g_running) return true;

    /* Clamp + convert amplitude/offset to Q12 (0..4095) */
    if (amplitude_0to1 < 0.0f) amplitude_0to1 = 0.0f;
    if (amplitude_0to1 > 1.0f) amplitude_0to1 = 1.0f;
    if (offset_0to1    < 0.0f) offset_0to1    = 0.0f;
    if (offset_0to1    > 1.0f) offset_0to1    = 1.0f;

    /* Guard: prevent clipping at extremes by leaving a little headroom */
    float max_amp = fminf(offset_0to1, 1.0f - offset_0to1);
    if (amplitude_0to1 > max_amp) amplitude_0to1 = max_amp;

    g_off_q12 = (uint16_t)lrintf(offset_0to1 * 4095.0f);
    g_amp_q12 = (uint16_t)lrintf(amplitude_0to1 * 2047.0f); /* scale on +/-2047 */

    /* Frequency / phase */
    g_freq_hz = freq_hz;
    g_phase = 0;
    g_phase_inc = compute_phase_inc(g_freq_hz, g_cfg.fs_hz);

    /* Prefill both halves of the circular buffer */
    nco_fill(&g_buf[0], CORE_DAC_SINE_CHUNK);
    nco_fill(&g_buf[CORE_DAC_SINE_CHUNK], CORE_DAC_SINE_CHUNK);

    /* Start DAC with DMA (HAL handles DMAMUX routing already configured) */
    uint32_t ch = (g_cfg.channel == CORE_DAC_CHANNEL2) ? DAC_CHANNEL_2 : DAC_CHANNEL_1;
    if (HAL_DAC_Start_DMA(&g_hdac, ch, (uint32_t*)g_buf, (uint32_t)(CORE_DAC_SINE_CHUNK * 2), DAC_ALIGN_12B_R) != HAL_OK)
        return false;

    /* Finally, start the timer (TRGO source) */
    if (HAL_TIM_Base_Start(&g_htim6) != HAL_OK) {
        HAL_DAC_Stop_DMA(&g_hdac, ch);
        return false;
    }

    g_running = true;
    return true;
}

void core_DAC_sine_stop(void) {
    if (!g_running) return;
    uint32_t ch = (g_cfg.channel == CORE_DAC_CHANNEL2) ? DAC_CHANNEL_2 : DAC_CHANNEL_1;

    HAL_TIM_Base_Stop(&g_htim6);
    HAL_DAC_Stop_DMA(&g_hdac, ch);

    g_running = false;
}

void core_DAC_sine_set_frequency(float freq_hz) {
    /* Click-free in NCO: just update phase_inc; takes effect on next fill */
    g_freq_hz = fminf(fmaxf(freq_hz, CORE_DAC_F_MIN_HZ), CORE_DAC_F_MAX_HZ);
    g_phase_inc = compute_phase_inc(g_freq_hz, g_cfg.fs_hz);
}

void core_DAC_sine_set_amp_offset(float amplitude_0to1, float offset_0to1) {
    if (amplitude_0to1 < 0.0f) amplitude_0to1 = 0.0f;
    if (amplitude_0to1 > 1.0f) amplitude_0to1 = 1.0f;
    if (offset_0to1    < 0.0f) offset_0to1    = 0.0f;
    if (offset_0to1    > 1.0f) offset_0to1    = 1.0f;

    float max_amp = fminf(offset_0to1, 1.0f - offset_0to1);
    if (amplitude_0to1 > max_amp) amplitude_0to1 = max_amp;

    g_off_q12 = (uint16_t)lrintf(offset_0to1 * 4095.0f);
    g_amp_q12 = (uint16_t)lrintf(amplitude_0to1 * 2047.0f);
}

bool  core_DAC_sine_is_running(void) { return g_running; }
float core_DAC_sine_get_frequency(void) { return g_freq_hz; }
uint32_t core_DAC_sine_get_fs(void) { return g_cfg.fs_hz ? g_cfg.fs_hz : CORE_DAC_SINE_FS_HZ; }
uint8_t  core_DAC_sine_get_channel(void) { return g_cfg.channel; }


/**
Interupt Handlers
Callbacks
*/

/* Use the HAL’s DAC DMA callbacks (keeps code portable & simple) */
void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *hdac) {
    (void)hdac;
    /* Fill first half */
    nco_fill(&g_buf[0], CORE_DAC_SINE_CHUNK);
}
void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac) {
    (void)hdac;
    /* Fill second half */
    nco_fill(&g_buf[CORE_DAC_SINE_CHUNK], CORE_DAC_SINE_CHUNK);
}
void HAL_DAC_ConvHalfCpltCallbackCh2(DAC_HandleTypeDef *hdac) {
    (void)hdac;
    nco_fill(&g_buf[0], CORE_DAC_SINE_CHUNK);
}
void HAL_DAC_ConvCpltCallbackCh2(DAC_HandleTypeDef *hdac) {
    (void)hdac;
    nco_fill(&g_buf[CORE_DAC_SINE_CHUNK], CORE_DAC_SINE_CHUNK);
}

/* Route the DMA channel IRQ to HAL */
void DMA1_Channel3_IRQHandler(void) { HAL_DMA_IRQHandler(&g_hdma); }
void DMA1_Channel4_IRQHandler(void) { HAL_DMA_IRQHandler(&g_hdma); }

/* Optional: error hooks */
void HAL_DAC_ErrorCallbackCh1(DAC_HandleTypeDef *hdac) { (void)hdac; /* add diagnostics if desired */ }
void HAL_DAC_ErrorCallbackCh2(DAC_HandleTypeDef *hdac) { (void)hdac; /* add diagnostics if desired */ }

