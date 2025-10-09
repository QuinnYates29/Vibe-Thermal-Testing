/*
Main.c -- Adjustable frequency using PA2 = UP and PA3 = DOWN
Wave starts between 20-100hz
Increments by 10hz with each button click
*/

#include "stm32g4xx_hal.h"
#include "dac.h"     /* your driver: core_DAC_* API */
#include <stdbool.h>
#include <stdint.h>

//user config
#define APP_FREQ_MIN_HZ      20.0f
#define APP_FREQ_MAX_HZ     100.0f
#define APP_FREQ_STEP_HZ     10.0f
#define APP_INIT_FREQ_HZ     50.0f
#define APP_INIT_AMP         0.95f
#define APP_INIT_OFFSET      0.50f

//DAC output pin: PA4 by default
#define DAC_PORT             GPIOA
#define DAC_PIN              GPIO_PIN_4
#define DAC_CHANNEL_LOGICAL  CORE_DAC_CHANNEL1

//Buttons : active low with pullups
#define BTN_PORT             GPIOA
#define BTN_UP_PIN           GPIO_PIN_2    /* increments +10 Hz */
#define BTN_DOWN_PIN         GPIO_PIN_3    /* decrements −10 Hz */
#define BTN_DEBOUNCE_MS      25u

//forward declerations
bool core_clock_init();
static void MX_GPIO_Init_Buttons(void);

typedef struct {
  GPIO_PinState stable;
  GPIO_PinState last_raw;
  uint32_t      last_change_ms;
} debounce_t;

static void debounce_init(debounce_t *d, GPIO_PinState initial);
static bool debounce_falling_edge(debounce_t *d, GPIO_PinState raw_now);
/*
**MAIN**
*/
int main(void)
{
    HAL_Init();
    core_clock_init();

    MX_GPIO_Init_Buttons();

    /**
    *DAC driver configuration
    */
    core_DAC_def_t dac_cfg = {
        .port               = DAC_PORT,
        .pin                = DAC_PIN,
        .allowed_connections= CORE_DAC_ALLOWED_DAC1, /* doc-only bitmask */
        .channel            = DAC_CHANNEL_LOGICAL,   /* CH1=PA4, CH2=PA5 */
        .trigger_tim        = TIM6,                  /* driver will init TIM6 as TRGO */
        .tim_input_hz       = 0,                     /* 0 => let driver infer from RCC */
        .fs_hz              = CORE_DAC_SINE_FS_HZ,   /* fixed sample rate (25 kHz default) */
        .lut_len            = CORE_DAC_SINE_LUT_LEN, /* 512 default */
        .dac                = DAC1,
        .running            = false
    };

    if (!core_DAC_sine_init(&dac_cfg)) {
        while (1) {/**if initialization fails it will infinite loop for debugging */}
    }

    /**
    *output initial frequency level
    */
    float freq_hz = APP_INIT_FREQ_HZ;
    if (!core_DAC_sine_start(freq_hz, APP_INIT_AMP, APP_INIT_OFFSET)) {
        while (1) { /* trap */ }
    }

    /**
    *Debounce options
    */
    debounce_t db_up, db_dn;
    debounce_init(&db_up, HAL_GPIO_ReadPin(BTN_PORT, BTN_UP_PIN));
    debounce_init(&db_dn, HAL_GPIO_ReadPin(BTN_PORT, BTN_DOWN_PIN));

    /**
    * Main Loop
    * Polls buttons for input and updates frequency when button is pressed
    */
     while (1) {
        /* Read raw pin levels (active-low buttons) */
        GPIO_PinState raw_up = HAL_GPIO_ReadPin(BTN_PORT, BTN_UP_PIN);
        GPIO_PinState raw_dn = HAL_GPIO_ReadPin(BTN_PORT, BTN_DOWN_PIN);

        /* Rising/falling detection with debounce (we act on falling edge = press) */
        bool up_pressed = debounce_falling_edge(&db_up, raw_up);
        bool dn_pressed = debounce_falling_edge(&db_dn, raw_dn);

        /* Adjust frequency in fixed-fs (NCO) mode with ±10 Hz steps */
        if (up_pressed) {
            float new_f = freq_hz + APP_FREQ_STEP_HZ;
            if (new_f > APP_FREQ_MAX_HZ) new_f = APP_FREQ_MAX_HZ;
            if (new_f != freq_hz) {
                freq_hz = new_f;
                core_DAC_sine_set_frequency(freq_hz);
            }
        }
        if (dn_pressed) {
            float new_f = freq_hz - APP_FREQ_STEP_HZ;
            if (new_f < APP_FREQ_MIN_HZ) new_f = APP_FREQ_MIN_HZ;
            if (new_f != freq_hz) {
                freq_hz = new_f;
                core_DAC_sine_set_frequency(freq_hz);
            }
        }
        
        HAL_Delay(2); //small delay to keep cpu from getting too hot
    }
}

/**
*Button Initialization
*/
static void MX_GPIO_Init_Buttons(void){
    /* Enable GPIOA clock (adjust if your buttons are on a different port) */
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gi = {0};
    gi.Mode  = GPIO_MODE_INPUT;
    gi.Pull  = GPIO_PULLUP;               /* active-low buttons with internal pull-ups */
    gi.Speed = GPIO_SPEED_FREQ_LOW;

    gi.Pin = BTN_UP_PIN;
    HAL_GPIO_Init(BTN_PORT, &gi);

    gi.Pin = BTN_DOWN_PIN;
    HAL_GPIO_Init(BTN_PORT, &gi);
}

static void debounce_init(debounce_t *d, GPIO_PinState initial)
{
    d->stable = d->last_raw = initial;
    d->last_change_ms = HAL_GetTick();
}

/* returns true on a debounced falling edge (HIGH->LOW), i.e., press for active-low */
static bool debounce_falling_edge(debounce_t *d, GPIO_PinState raw_now)
{
    uint32_t now = HAL_GetTick();

    if (raw_now != d->last_raw) {
        d->last_raw = raw_now;
        d->last_change_ms = now;
    }

    /* if raw has been different from stable long enough, adopt it */
    if ((now - d->last_change_ms) >= BTN_DEBOUNCE_MS && d->stable != d->last_raw) {
        GPIO_PinState prev_stable = d->stable;
        d->stable = d->last_raw;

        /* detect falling edge (HIGH -> LOW) = button press (active-low) */
        if (prev_stable == GPIO_PIN_SET && d->stable == GPIO_PIN_RESET) {
            return true;
        }
    }
    return false;
}
