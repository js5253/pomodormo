#include "Arduino.h"
#include <freertos/FreeRTOS.h>
#include <driver/timer.h>

#include "Util.hpp"

class TimerTask : FreeRTOSTask
{
public:
    void setup() override
    {
    }
    void loop(void *params) override
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
                timerAttachInterrupt(myTimer, this->_makeIdle, false);
                timerAlarmWrite(myTimer, 1000000, true);
                timerStart(myTimer);

                // here, make an alarm later on
                break;
            }
            xSemaphoreGive(mLeftTime);
        }
    }

private:
    hw_timer_t *myTimer;
    void ARDUINO_ISR_ATTR onTimer()
    {
        xSemaphoreTake(mLeftTime, portMAX_DELAY);
        xSemaphoreTake(mElapsedTime, portMAX_DELAY);
        --timeLeft;
        ++timeElapsed;
        xSemaphoreGive(mElapsedTime);
        xSemaphoreGive(mLeftTime);
    }
    void ARDUINO_ISR_ATTR _makeIdle()
    {
        setSystemState(AppState::IDLE);
    }
};
