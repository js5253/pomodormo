#include <freertos/FreeRTOS.h>
#include <Arduino.h>
#include "Util.h"

volatile AppState globalState = AppState::INIT;
SemaphoreHandle_t mGlobalState;

AppState getSystemState(void) {
    AppState state;
    if (xSemaphoreTake(mGlobalState, portMAX_DELAY) == pdTRUE) {
        state = globalState;
        xSemaphoreGive(mGlobalState);
    }
    return state;
}
void setSystemState(AppState state) {
    if (xSemaphoreTake(mGlobalState, portMAX_DELAY) == pdTRUE) {
        globalState = state;
        xSemaphoreGive(mGlobalState);
    }
}