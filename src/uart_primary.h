#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// ===========================================================================
//  uart_primary.h — SK01 UART state machine (Task A)
// ===========================================================================

// Call once from setup() after RTOS primitives are created.
// Spawns PRIMARY_UART_TASK on Core 0.
void uart_primary_init(SemaphoreHandle_t ldpcn_sem);
