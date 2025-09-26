#include "FreeRTOS.h"
#include "task.h"

/* Called if a task overflows its stack (configCHECK_FOR_STACK_OVERFLOW > 0). */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;

    /* Mask interrupts using FreeRTOS port macro (portable, no CMSIS needed) */
    taskDISABLE_INTERRUPTS();

    /* Optional: breakpoint for debugging (remove if you don't want it) */
    __asm volatile ("bkpt #0");

    for (;;) { /* stay here */ }
}

/* Called if pvPortMalloc() returns NULL (configUSE_MALLOC_FAILED_HOOK == 1). */
void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    __asm volatile ("bkpt #0");
    for (;;) { }
}

/* Optional hooks (only if enabled in FreeRTOSConfig.h) */
void vApplicationIdleHook(void) { }
void vApplicationTickHook(void) { }

