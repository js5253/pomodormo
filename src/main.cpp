#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <optional>
#include <Preferences.h>
#include <memory>
#include <freertos/FreeRTOS.h>
#include <Wire.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <driver/i2s.h>

#include "Util.h"
#include "IMUTask.hpp"
#include "DisplayTask.hpp"
#include "TimerTask.hpp"
#include "NetworkTask.hpp"

Preferences preferences;

IMUTask *imuTask;
DisplayTask *displayTask;
NetworkTask *networkTask;
TimerTask *timerTask;

void setup()
{
  Serial.begin(115200);
  setCpuFrequencyMhz(80); /// TODO: implement better power-saving methods.
  // get default minute mappings
  imuTask = IMUTask::getInstance();
  displayTask = DisplayTask::getInstance();
  networkTask = NetworkTask::getInstance();
  timerTask = TimerTask::getInstance();

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

  // initialize all tasks' setup requirements:
  imuTask->setup();
  displayTask->setup();
  networkTask->setup();
  timerTask->setup();
  // end default minute mappings

  qOrientationChange = xQueueCreate(1000, sizeof(GyroMessageType));

  xTaskCreate(imuTask->loop, "IMUTask", 1000, NULL, 2, &tIMU);
  xTaskCreate(networkTask->loop, "NetworkTask", 1000, NULL, 4, &tNetwork);
  xTaskCreate(timerTask->loop, "TimerTask", 1000, NULL, 1, &tTimer);
}
void loop()
{
  // do nothing; we're using FreeRTOS
}