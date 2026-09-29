#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <optional>
#include <driver/timer.h>
#include <Preferences.h>
#include <memory>
#include <freertos/FreeRTOS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <driver/i2s.h>
#include "Util.h"
#include <Audio.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define I2S_DOUT 22
#define I2S_BCLK 26
#define I2S_LRC 25
// CONFIGURE THESE VALUES!

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

SemaphoreHandle_t mElapsedTime; //xSemaphoreCreateMutex();
SemaphoreHandle_t mLeftTime; //xSemaphoreCreateMutex();

hw_timer_t *myTimer = NULL;

AsyncWebServer server(80);

TaskHandle_t tDisplay;
TaskHandle_t tIMU;
TaskHandle_t tNetwork;
TaskHandle_t tTimer;

Preferences preferences;
Audio audio;

const char *SSID = "POMODORMO";
const char *PASSWORD = "POMODORMO";

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

void ARDUINO_ISR_ATTR onTimer()
{
  xSemaphoreTake(mLeftTime, portMAX_DELAY);
  xSemaphoreTake(mElapsedTime, portMAX_DELAY);
  --timeLeft;
  ++timeElapsed;
  xSemaphoreGive(mElapsedTime);
  xSemaphoreGive(mLeftTime);
}
void ARDUINO_ISR_ATTR makeIdle()
{
  setSystemState(AppState::IDLE);
}

void tfDisplay(void *params)
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(17);
  audio.connecttoFS(LittleFS, "/alarm.wav");
  int prevDisplayTime;
  while (true)
  {
    AppState state = getSystemState();
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
        display.printf("WORKING: %d", mElapsedTime);
      }
      prevDisplayTime = timeElapsed;
      xSemaphoreGive(mElapsedTime);
      break;

    case BREAKING:
      xSemaphoreTake(mElapsedTime, portMAX_DELAY);
      if (prevDisplayTime != timeElapsed)
      {
        display.clearDisplay();
        display.printf("BREAK: %d", mElapsedTime);
      }
      prevDisplayTime = timeElapsed;
      xSemaphoreGive(mElapsedTime);
      break;
    case FINISHED:
      display.clearDisplay();
      display.printf("TIME!");
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
  mpu.setTemperatureStandby(true);

  while (true)
  {
    AppState state = getSystemState();
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
        GyroMessage g = GyroMessage {.msg = msg};
        xQueueSend(qOrientationChange, (void *)&g, 0);
      }
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void tfTimer(void *params)
{
  GyroMessage messageEvent;
  while (true)
  {
    AppState state = getSystemState();
    xSemaphoreTake(mLeftTime, portMAX_DELAY);
    if (xQueueReceive(qOrientationChange, &(messageEvent), portMAX_DELAY) == pdPASS)
    {
      // reset timer

      switch (messageEvent.msg)
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
      myTimer = timerBegin(TIMER_0, 2, false); /// FIXME: check if this is an appropriate prescaler value
      timerAttachInterrupt(myTimer, &onTimer, false);
      timerAlarmWrite(myTimer, 1000000, true);
      timerStart(myTimer);

      xSemaphoreTake(mElapsedTime, portMAX_DELAY);
      timeElapsed = 0;
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

void onRequest(AsyncWebServerRequest *request)
{
  // Handle Unknown Request
  request->send(404);
}

void tfNetwork(void *params)
{
  LittleFS.begin(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  server.onNotFound(onRequest);
  server.serveStatic("/page.htm", LittleFS, "/www/index.html").setDefaultFile("/www/index.html");
  server.begin();
  /// TODO: check that AsyncWebServer works well this way/under FreeRTOS
  while (true)
  {
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void setup()
{
  Serial.begin(115200);
  setCpuFrequencyMhz(80); /// TODO: implement better power-saving methods.
  // get default minute mappings
  initGlobalState();
  mElapsedTime = xSemaphoreCreateMutex();
  mLeftTime = xSemaphoreCreateMutex();
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

  // xTaskCreate(tfDisplay, "DisplayTask", 1000, NULL, 3, &tDisplay);
  // xTaskCreate(tfIMU, "IMUTask", 1000, NULL, 2, &tIMU);
  // xTaskCreate(tfNetwork, "NetworkTask", 1000, NULL, 4, &tNetwork);
  xTaskCreate(tfTimer, "TimerTask", 1000, NULL, 1, &tTimer);
}
void loop()
{
  // do nothing; we're using FreeRTOS
}