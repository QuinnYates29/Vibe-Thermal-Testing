#include "stm32g4xx_hal.h"
#include "dac_sine.h"

int main(void)
{
  SineGen_Init();

  for (;;)
  {
    SineGen_Task();    /* reads pot and updates frequency */
    HAL_Delay(5);
  }
}

