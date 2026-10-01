#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <freertos/FreeRTOS.h>
#include "Util.hpp"

class IMUTask : FreeRTOSTask
{
    public:
    void setup() override
    {
        ESP_ERROR_CHECK(_mpu.begin());
        _mpu.setGyroRange(MPU6050_RANGE_1000_DEG);
        _mpu.setAccelerometerStandby(true, true, true);
        _mpu.setTemperatureStandby(true);
    }
    void loop(void* params) override
    {
        sensors_event_t* accel;
        sensors_event_t* gyro;
        sensors_event_t* temp;

        sensors_event_t *prevGyro;
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
                _mpu.getEvent(accel, gyro, temp);
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
                    auto g = new GyroMessage{.msg = msg};
                    xQueueSend(qOrientationChange, g, 0);
                }
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

private:
    Adafruit_MPU6050 _mpu;
};
