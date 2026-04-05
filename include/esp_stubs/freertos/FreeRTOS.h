#pragma once
#include <stdint.h>

typedef uint32_t TickType_t;
typedef uint32_t UBaseType_t;
typedef int      BaseType_t;

#define pdTRUE  1
#define pdFALSE 0
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define portMAX_DELAY     ((TickType_t)0xffffffffUL)
#define configTICK_RATE_HZ 1000
