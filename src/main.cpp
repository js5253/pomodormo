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
volatile int timeLeft = 0;

QueueHandle_t qOrientationChange;

Adafruit_MPU6050 mpu;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

SemaphoreHandle_t mElapsedTime = xSemaphoreCreateMutex();
SemaphoreHandle_t mLeftTime = xSemaphoreCreateMutex();
SemaphoreHandle_t mGlobalState = xSemaphoreCreateMutex();

hw_timer_t *myTimer = NULL;

TaskHandle_t tDisplay;
TaskHandle_t tIMU;
TaskHandle_t tNetwork;
TaskHandle_t tTimer;

Preferences preferences;

enum GyroMessageType
{
  CYCLE_FLIPPED_NORMAL,
  CYCLE_FLIPPED_COUNT,
  CYCLE_FLIPPED_INV,
  CYCLE_FLIPPED_CLOCK
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

void ARDUINO_ISR_ATTR onTimer()
{
  xSemaphoreTake(mElapsedTime, portMAX_DELAY);
  --timeElapsed;
  xSemaphoreGive(mElapsedTime);
}
void ARDUINO_ISR_ATTR makeIdle()
{
  xSemaphoreTake(mGlobalState, portMAX_DELAY);
  setSystemState(AppState::IDLE);
  xSemaphoreGive(mGlobalState);
}

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
    [[fallthrough]]
    case WORKING:
    [[fallthrough]]
    case IDLE:
    [[fallthrough]]
    case BREAKING:
      mpu.getEvent(accel, gyro, temp);
      if (prevGyro != gyro)
      {
        int degree = gyro->gyro.roll;
        /// FIXME: add correct gyro and degree ratings
        GyroMessageType msg = GyroMessageType::CYCLE_FLIPPED_NORMAL;
        if (degree < 90)
        {
          msg = GyroMessageType::CYCLE_FLIPPED_CLOCK;
        }
        else if (
            degree < 0)
        {
          msg = GyroMessageType::CYCLE_FLIPPED_COUNT;
        }
        else if (degree < 90)
        {
          msg = GyroMessageType::CYCLE_FLIPPED_INV;
        }
        xQueueSend(qOrientationChange, (void *)new GyroMessage{.msg = msg}, 0);
      }
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void tfTimer(void *params)
{
  GyroMessage *messageEvent;
  while (true)
  {
    AppState state = get_system_state();
    if (xQueueReceive(qOrientationChange, &(messageEvent), portMAX_DELAY) == pdPASS)
    {
      // reset timer
      switch (messageEvent->msg)
      {
      case CYCLE_FLIPPED_NORMAL:
        timeLeft = minutes.normal;
        break;
      case CYCLE_FLIPPED_COUNT:
        timeLeft = minutes.count_90;
        break;
      case CYCLE_FLIPPED_INV:
        timeLeft = minutes.inverted;
        break;
      case CYCLE_FLIPPED_CLOCK:
        timeLeft = minutes.clock_90;
        break;
      }
      xSemaphoreTake(mLeftTime, 0);
      state = AppState::WORKING;
      printf("Time Change!");
      myTimer = timerBegin(1000000, 2, false);
      timerAttachInterrupt(myTimer, &onTimer, false);
      timerAlarmWrite(myTimer, 1000000, true);
      timerStart(myTimer);

      xSemaphoreTake(mElapsedTime, portMAX_DELAY);
      timeElapsed = 0;
      xSemaphoreTake(mLeftTime, portMAX_DELAY);
      delete messageEvent;
    };
    switch (state)
    {
    case INIT:
      break;
    case IDLE:
      break;
    case WORKING:
      if (timeLeft == 0)
      {
        timerStop(myTimer);
        timerStart(myTimer);
        state = BREAKING;
      }
      break;
    case BREAKING:
      if (timeLeft == 0)
      {
        timerStop(myTimer);
        state = FINISHED;
      }
      break;

    case FINISHED:
      timerAttachInterrupt(myTimer, &makeIdle, false);
      timerAlarmWrite(myTimer, 1000000, true);
      timerStart(myTimer);

      // here, make an alarm later on
      break;
    }
    xSemaphoreGive(mLeftTime);

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