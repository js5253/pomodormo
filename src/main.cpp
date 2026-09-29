#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <optional>
#include "driver/timer.h"
#include <Preferences.h>

enum AppState
{
  INIT,
  IDLE,
  WORKING,
  BREAKING,
};

struct OrientationMinuteMappings
{
  int normal;
  int count_90;
  int inverted;
  int clock_90;
};

volatile AppState state = AppState::INIT;

QueueHandle_t qOrientationChange;

Adafruit_MPU6050 mpu;
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
  while (true)
  {
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
void tfIMU(void *params)
{
  // setup
  int mpuResult = mpu.begin();
  ESP_ERROR_CHECK(mpuResult);
  sensors_event_t *accel;
  sensors_event_t *gyro;
  sensors_event_t *temp;

  sensors_event_t *prevGyro;

  mpu.setGyroRange(MPU6050_RANGE_1000_DEG);
  mpu.setAccelerometerStandby(true, true, true);

  while (true)
  {
    mpu.getEvent(accel, gyro, temp);
    if (prevGyro->gyro.x > gyro->gyro.x)
    { // CHANGE THIS TO ACTUALLY MATCH FLIP MOTIONS...
      // fire off event here
      xQueueSend(qOrientationChange, (void *)new GyroMessage{.msg = GyroMessageType::CYCLE_FLIPPED}, 0);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void tfTimer(void *params)
{
  GyroMessage *messageEvent;
  while (true)
  {
    if (xQueueReceive(qOrientationChange, &(messageEvent), (TickType_t)10) == pdPASS)
    {
      printf("Time Change!");
    };
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
  Serial.begin(11250);
  preferences.begin("config", false);
  // get default minute mappings
  minutes = OrientationMinuteMappings{
      .normal = preferences.getInt("normal", 5),
      .count_90 = preferences.getInt("count_90", 10),
      .inverted = preferences.getInt("inverted", 20),
      .clock_90 = preferences.getInt("clock_90", 55),
  };
  // end default minute mappings
  qOrientationChange = xQueueCreate(1000, sizeof(GyroMessageType));
  xTaskCreate(tfDisplay, "DisplayTask", 1000, NULL, 1, &tDisplay);
  xTaskCreate(tfIMU, "IMUTask", 1000, NULL, 1, &tIMU);
  xTaskCreate(tfNetwork, "NetworkTask", 1000, NULL, 1, &tNetwork);
  xTaskCreate(tfTimer, "TimerTask", 1000, NULL, 1, &tTimer);
}