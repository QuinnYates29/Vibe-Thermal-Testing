#include "main.h"

#include <stdbool.h>
#include <stdio.h>

#include "can.h"
#include "clock.h"
#include "gpio.h"
#include "error_handler.h"
#include "core_config.h"
#include "adc.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include <stm32g4xx_hal.h>


void blink(void *pvParameters) {
    (void) pvParameters;
    while(true) {
        core_GPIO_digital_write(GPIOA, GPIO_PIN_8, true);
        vTaskDelay(500);
        core_GPIO_digital_write(GPIOA, GPIO_PIN_8, false); 
        vTaskDelay(1000 * portTICK_PERIOD_MS);
    }
}

void blink2(void *pvParameters) {
    (void) pvParameters;
    while(true) {
        core_GPIO_digital_write(GPIOA, GPIO_PIN_8, true);
        vTaskDelay(500);
        core_GPIO_digital_write(GPIOA, GPIO_PIN_8, false); 
        vTaskDelay(1000 * portTICK_PERIOD_MS);
    }
}

void button_led(void *pvParameters){
    (void) pvParameters;
    while(true){
        if(core_GPIO_digital_read(GPIOA, GPIO_PIN_2) == true){
            core_GPIO_digital_write(GPIOA, GPIO_PIN_8, false);
            vTaskDelay(125);
        }
        else{
            core_GPIO_digital_write(GPIOA, GPIO_PIN_8, true);
            vTaskDelay(125);
        }
    }
}

void button_led2(void *pvParameters){
    (void) pvParameters;
    while(true){
        if(core_GPIO_digital_read(GPIOA, GPIO_PIN_3) == true){
            core_GPIO_digital_write(GPIOA, GPIO_PIN_0, false);
            vTaskDelay(125);
        }
        else{
            core_GPIO_digital_write(GPIOA, GPIO_PIN_0, true);
            vTaskDelay(125);
        }
    }
}

void print_pot(void *pvParameters) {
    (void) pvParameters;
    uint16_t adc_value;
    while(true) {
	    if (core_ADC_read_channel(GPIOA, GPIO_PIN_1, &adc_value)) {
            printf("ADC value: %u\n", adc_value);
	        vTaskDelay(1000);
	    }
    }
}

int main(void) {
    HAL_Init();

    // Drivers
    
    core_GPIO_init(GPIOA, GPIO_PIN_8, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL);
    core_GPIO_init(GPIOA, GPIO_PIN_0, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL);
    core_GPIO_init(GPIOA, GPIO_PIN_2, GPIO_MODE_INPUT, GPIO_PULLUP);
    core_GPIO_init(GPIOA, GPIO_PIN_3, GPIO_MODE_INPUT, GPIO_PULLUP);
    core_GPIO_set_heartbeat(GPIO_PIN_RESET);

    core_ADC_init(ADC1);
    core_ADC_init(GPIOA, GPIO_PIN_1, 0);

    if (!core_clock_init()) error_handler();
    if (!core_CAN_init(FDCAN1, 1000000)) error_handler();

    int err = xTaskCreate(button_led, "b1", 1000, NULL, 3, NULL);
    err = xTaskCreate(button_led2, "b2", 1000, NULL, 3, NULL);
    err = xTaskCreate(print_pot, "print_pot", 1000, NULL, 3, NULL);

    if (err != pdPASS) {
        error_handler();
    }  

    NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

    // hand control over to FreeRTOS
    vTaskStartScheduler();

    // we should not get here ever
    error_handler();
    return 1;
}

// Called when stack overflows from rtos
// Not needed in header, since included in FreeRTOS-Kernel/include/task.h
void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName) {
    (void) xTask;
    (void) pcTaskName;

    error_handler();
}
