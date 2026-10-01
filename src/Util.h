

#pragma once

enum AppState
{
  INIT,
  IDLE,
  WORKING,
  BREAKING,
  FINISHED
};
void initGlobalState(void);
AppState getSystemState(void);
void setSystemState(AppState state);

class FreeRTOSTask
{
public:
  virtual FreeRTOSTask *getInstance();
  virtual void setup();
  virtual void loop(void *params);

protected:
  FreeRTOSTask *_singleton;
};

SemaphoreHandle_t mElapsedTime; // xSemaphoreCreateMutex();
SemaphoreHandle_t mLeftTime;    // xSemaphoreCreateMutex();

volatile int timeElapsed = 0;
volatile int timeLeft = 0;

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

struct OrientationMinuteMappings
{
  int normal;
  int count_90;
  int inverted;
  int clock_90;
};

OrientationMinuteMappings minutes = {
    .normal = 5,
    .count_90 = 10,
    .inverted = 20,
    .clock_90 = 55,
};

QueueHandle_t qOrientationChange;
