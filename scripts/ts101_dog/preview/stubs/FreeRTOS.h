#pragma once
// Host preview stub: just enough of FreeRTOS for the UI drawing code
#include "FreeRTOSConfig.h"
#include <stdint.h>
typedef uint32_t TickType_t;
typedef void    *TaskHandle_t;
extern TickType_t previewTicks;
static inline TickType_t xTaskGetTickCount() { return previewTicks; }
static inline void       vTaskDelayUntil(TickType_t *, TickType_t) {}
