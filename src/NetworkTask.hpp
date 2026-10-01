#pragma once

#include "Util.hpp"

class NetworkTask : FreeRTOSTask {
    public:
    void setup() override;
    void loop(void* params) override;
    NetworkTask* getInstance();
};