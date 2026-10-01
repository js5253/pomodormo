#include "Util.h"
#include "LittleFS.h"
#include "WiFi.h"
#include "AsyncTCP.h"
#include "ESPAsyncWebServer.h"

const char *SSID = "POMODOMO";
const char *PASSWORD = "POMODOMO";

class NetworkTask : FreeRTOSTask
{
public:
    AsyncWebServer server;
    void setup()
    {
        server = AsyncWebServer(80);
        LittleFS.begin(true);
        WiFi.mode(WIFI_STA);
        WiFi.begin(SSID, PASSWORD);
        server.onNotFound(onRequest);
        server.serveStatic("/page.htm", LittleFS, "/www/index.html").setDefaultFile("/www/index.html");
        server.begin();
    }
    void loop()
    {
    vTaskDelay(pdMS_TO_TICKS(100));
    }

private:
    void onRequest(AsyncWebServerRequest *request)
    {
        // Handle Unknown Request
        request->send(404);
    }
};
