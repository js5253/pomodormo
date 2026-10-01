#pragma once

#include "Util.hpp"

class TimerTask : FreeRTOSTask {
    public:
    void setup() override;
    void loop(void* params) override;
    TimerTask* getInstance();
};