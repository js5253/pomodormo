#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <optional>
#include "driver/timer.h"
#include <Preferences.h>
#include <memory>
#include <freertos/FreeRTOS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Util.h"
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

struct OrientationMinuteMappings
{
  int normal;
  int count_90;
  int inverted;
  int clock_90;
};

volatile int timeElapsed = 0;

QueueHandle_t qOrientationChange;

Adafruit_MPU6050 mpu;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

SemaphoreHandle_t mElapsedTime = xSemaphoreCreateMutex();
SemaphoreHandle_t mGlobalState = xSemaphoreCreateMutex();

TaskHandle_t tDisplay;
TaskHandle_t tIMU;
TaskHandle_t tNetwork;
TaskHandle_t tTimer;

Preferences preferences;

enum GyroMessageType
{
  CYCLE_FLIPPED
};
struct GyroMessage
{
  GyroMessageType msg;
};

OrientationMinuteMappings minutes = {
    .normal = 5,
    .count_90 = 10,
    .inverted = 20,
    .clock_90 = 55,
};

timer_config_t config = {
    .alarm_en = TIMER_ALARM_EN,
    .counter_en = TIMER_PAUSE,
    .intr_type = TIMER_INTR_LEVEL,
    .divider = 2, // CHANGEME
};

void tfDisplay(void *params)
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  int prevDisplayTime;
  while (true)
  {
    AppState state = get_system_state();
    switch (state)
    {
    case INIT:
      ESP_ERROR_CHECK(display.begin());
      break;
    case WORKING:
      xSemaphoreTake(mElapsedTime, portMAX_DELAY);
      if (prevDisplayTime != timeElapsed)
      {
        display.clearDisplay();
        display.printf("Elapsed: %d", mElapsedTime);
      }
      prevDisplayTime = timeElapsed;
      xSemaphoreGive(mElapsedTime);
      break;

    case BREAKING:
      xSemaphoreTake(mElapsedTime, portMAX_DELAY);
      if (prevDisplayTime != timeElapsed)
      {
        display.clearDisplay();
        display.printf("Elapsed: %d", mElapsedTime);
      }
      prevDisplayTime = timeElapsed;
      xSemaphoreGive(mElapsedTime);
      break;
    case FINISHED:
      display.clearDisplay();
      display.printf("FINISHED TIMER!");
      break;
    case IDLE:
      // do something?
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
void tfIMU(void *params)
{
  // setup
  ESP_ERROR_CHECK(mpu.begin());

  sensors_event_t *accel;
  sensors_event_t *gyro;
  sensors_event_t *temp;

  sensors_event_t *prevGyro;

  mpu.setGyroRange(MPU6050_RANGE_1000_DEG);
  mpu.setAccelerometerStandby(true, true, true);

  while (true)
  {
    AppState state = get_system_state();
    switch (state)
    {
    case INIT:
      // do something
      break;
    case WORKING:
    case IDLE:
    case BREAKING:
      mpu.getEvent(accel, gyro, temp);
      if (prevGyro != gyro)
      {
        int degree = gyro->gyro.roll;
        /// FIXME: add correct gyro and degree ratings
        if (degree < 90)
        {
        }
        else if (
            degree < 0)
        {
        }
        else if (degree < 90)
        {
        }
        xQueueSend(qOrientationChange, (void *)new GyroMessage{.msg = GyroMessageType::CYCLE_FLIPPED}, 0);
      }
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
void resetTimer()
{
}
void tfTimer(void *params)
{
  GyroMessage *messageEvent;
  while (true)
  {
    AppState state = get_system_state();
    if (xQueueReceive(qOrientationChange, &(messageEvent), (TickType_t)10) == pdPASS)
    {
      state = AppState::WORKING;
      printf("Time Change!");
      xSemaphoreTake(mElapsedTime, 0);
      timeElapsed = 0;
      xSemaphoreGive(mElapsedTime);
      delete messageEvent;
    };
    switch (state)
    {
    case INIT:
      break;
    case IDLE:
      break;
    case WORKING:
      if (timeElapsed == 0)
      {
        state = BREAKING;
      }
      break;
    case BREAKING:
      if (timeElapsed == 0)
      {
        state = IDLE;
      }
      break;

    case FINISHED:
      break;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
void tfNetwork(void *params)
{
  while (true)
  {
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void setup()
{
  Serial.begin(112500);
  setCpuFrequencyMhz(240);
  // get default minute mappings
  preferences.begin("config", false);
  minutes = OrientationMinuteMappings{
      .normal = preferences.getInt("normal", 5),
      .count_90 = preferences.getInt("count_90", 10),
      .inverted = preferences.getInt("inverted", 20),
      .clock_90 = preferences.getInt("clock_90", 55),
  };
  preferences.end();
  // end default minute mappings
  qOrientationChange = xQueueCreate(1000, sizeof(GyroMessageType));

  xTaskCreate(tfDisplay, "DisplayTask", 1000, NULL, 3, &tDisplay);
  xTaskCreate(tfIMU, "IMUTask", 1000, NULL, 2, &tIMU);
  xTaskCreate(tfNetwork, "NetworkTask", 1000, NULL, 4, &tNetwork);
  xTaskCreate(tfTimer, "TimerTask", 1000, NULL, 1, &tTimer);
}
void loop()
{
  // do nothing; we're using FreeRTOS
}