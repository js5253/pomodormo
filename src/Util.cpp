#include <freertos/FreeRTOS.h>
#include <Arduino.h>
#include "Util.h"

volatile AppState globalState = AppState::INIT;
SemaphoreHandle_t mGlobalState;

AppState get_system_state(void) {
    AppState state;
    if (xSemaphoreTake(mGlobalState, portMAX_DELAY) == pdTRUE) {
        state = globalState;
        xSemaphoreGive(mGlobalState);
    }
    return state;
}