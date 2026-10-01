#pragma once
#include "Util.h"
class DisplayTask : FreeRTOSTask {
    public:
    void setup();
    void loop(void* params);
    FreeRTOSTask* getInstance();

    protected:
    FreeRTOSTask* _singleton;
};