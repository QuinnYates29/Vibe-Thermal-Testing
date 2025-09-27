#include "stm32g4xx_hal.h"
#include "dac_sine.h"

int main(void)
{
  /* Initialize HAL and sine generator:
     - Builds sine LUT
     - Configures DAC1 on PA4 with DMA + TIM6
     - Configures buttons on PA2 (UP) and PA3 (DOWN) */
  SineGen_Init();

  /* Main loop: poll buttons every 5 ms and adjust output frequency
     between 20 Hz and 100 Hz in 10 Hz steps */
  for (;;)
  {
    SineGen_Task();
    HAL_Delay(5);
  }
}
