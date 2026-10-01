#include <Wire.h>
#include <Audio.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LittleFS.h>
#include "Util.hpp"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define I2S_DOUT 22
#define I2S_BCLK 26
#define I2S_LRC 25

class DisplayTask : FreeRTOSTask
{
private:
    static Adafruit_SSD1306 display;
    static Audio audio;
    static int prevDisplayTime;

protected:
    DisplayTask() {};

public:
    DisplayTask(DisplayTask &other) = delete;
    DisplayTask *_singleton = nullptr;
    DisplayTask *DisplayTask::GetInstance()
    {
        if (_singleton == nullptr)
        {
            _singleton = new DisplayTask();
        }
        return _singleton;
    }
  
    void setup() override
    {
        this->display = Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(0, 10);

        audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
        audio.setVolume(17);
        audio.connecttoFS(LittleFS, "/alarm.wav");
        int prevDisplayTime;
    };
    void loop(void* params) override
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
    };
};