#pragma once

#include "Util.hpp"
class DisplayTask : FreeRTOSTask {
    public:
    void setup() override;
    void loop(void* params) override;
    DisplayTask* getInstance();

    protected:
    DisplayTask* _singleton;
};