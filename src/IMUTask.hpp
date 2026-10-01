#pragma once

#include "Util.hpp"

class IMUTask : FreeRTOSTask {
    public:
    void setup() override;
    void loop(void* params) override;
    IMUTask* getInstance();
};