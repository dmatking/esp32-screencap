#pragma once
#include <unistd.h>
#include "FreeRTOS.h"

static inline void vTaskDelay(TickType_t ticks)
{
    usleep((useconds_t)ticks * 1000);   /* 1 tick = 1 ms */
}

#define vTaskDelayUntil(pxPreviousWakeTime, xTimeIncrement) \
    vTaskDelay(xTimeIncrement)

typedef void *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);

static inline int xTaskCreate(TaskFunction_t fn, const char *name,
                               uint32_t stack, void *param,
                               UBaseType_t prio, TaskHandle_t *handle)
{
    (void)name; (void)stack; (void)prio; (void)handle;
    fn(param);  /* run inline — fine for single-frame sim */
    return pdTRUE;
}
