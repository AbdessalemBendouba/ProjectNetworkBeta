#ifndef COMMUNICATION_HUB_H
#define COMMUNICATION_HUB_H

#include <Arduino.h>
#include <WiFi.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h" 
#include "Protocol.h"

class CommunicationHub {
    public:
        CommunicationHub(int CommunicationPort = 8888, int MaxSensorsCount = 2);
        void begin(QueueHandle_t vQ, QueueHandle_t cQ, const char* ssid = "ESP Server Access Point" , const char* password = "123456789");
    
    private:
        int _CommunicationPort;
        int _MaxSensorsCount;
        WiFiServer _tcpServer;
		WiFiClient* _clients;
        
        QueueHandle_t _vibQueue;
        QueueHandle_t _curQueue;
		
        void connectionWorker();
		
        static void taskWrapper(void* pvParameters);
};

#endif