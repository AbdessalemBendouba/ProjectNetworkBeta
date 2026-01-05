#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <arduinoFFT.h>
#include "esp_task_wdt.h" 

#include "Protocol.h"
#include "CommunicationHub.h"
#include "ProcessingCore.h"
#include "WebCode.h"

#define AGGREGATION_FACTOR 4  

const int TCP_PORT = 8888;
const int MAX_SENSORS = 2;

const int EVENT_PORT = 80;
const char* EVENT_PATH = "/events";

CommunicationHub SensHub;		//internal initialization (2 max @ 8888)
ProcessingCore SignalProcessor; //4 * 256 samples to "/events" @ 80

QueueHandle_t vibQueue; 
QueueHandle_t curQueue; 


void setup() {
    Serial.begin(115200);

    vibQueue = xQueueCreate(AGGREGATION_FACTOR, sizeof(InternalMessage_t));
    curQueue = xQueueCreate(AGGREGATION_FACTOR, sizeof(InternalMessage_t));

	SensHub.begin(vibQueue, curQueue);			// Begin Task 01 (Connection)
    SignalProcessor.begin(vibQueue, curQueue);	// Begin Task 02 (Processing)

}

void loop() { 
    vTaskDelete(NULL);	// The loop is empty because we use FreeRTOS tasks.
}

