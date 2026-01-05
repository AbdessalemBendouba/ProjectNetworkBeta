#include <Arduino.h>
#include "CommunicationHub.h"

CommunicationHub::CommunicationHub(int port, int max) : _tcpServer(port), _CommunicationPort(port), _MaxSensorsCount(max) {
    _clients = new WiFiClient[_MaxSensorsCount];
}

void CommunicationHub::taskWrapper(void* pvParameters) {
	CommunicationHub* instance = (CommunicationHub*) pvParameters; 
    instance->connectionWorker();
}

void CommunicationHub::begin(QueueHandle_t vQ, QueueHandle_t cQ, const char* ssid, const char* password){
	
	WiFi.softAP(ssid, password);
    Serial.print("Access Point Started. IP: "); 
    Serial.println(WiFi.softAPIP());
	
	_vibQueue = vQ;
	_curQueue = cQ;
	
	_tcpServer.begin();
    _tcpServer.setNoDelay(true);
	
	xTaskCreatePinnedToCore(
        taskWrapper,    // Function to run
        "ConnTask",     // Name
        10000,           // Stack size
        this,           // PASSING THE INSTANCE
        1,              // Priority
        NULL,           // Task handle
        0               // Core 0
    );
	
}

void CommunicationHub::connectionWorker() {
    DataPacket_t tempPacket; 
    InternalMessage_t qMsg;    

    for(;;) {
        // --- Accept New Clients ---
        if (_tcpServer.hasClient()) {
            WiFiClient newClient = _tcpServer.available();
            bool assigned = false;
            // Find an empty slot
            for (int i = 0; i < _MaxSensorsCount; i++) {
                if (!_clients[i] || !_clients[i].connected()) {
                    _clients[i] = newClient;
                    Serial.printf("Client connected to slot %d\n", i);
                    assigned = true;
                    break;
                }
            }
            if (!assigned) {
                newClient.stop(); // No room
            }
        }

        // --- Read Data from Connected Clients ---
        for (int i = 0; i < _MaxSensorsCount; i++) {
            if (_clients[i] && _clients[i].connected()) {
                
                // Check if we have a FULL packet (1028 bytes)
                if (_clients[i].available() >= sizeof(DataPacket_t)) {
                    
                    // Read the binary struct directly
                    _clients[i].readBytes((char*)&tempPacket, sizeof(DataPacket_t));

                    // Verify Header to ensure data integrity
                    if (tempPacket.header == PACKET_HEADER) {
                        
                        // Map to internal message
                        if (tempPacket.type == TYPE_VIBRATION) qMsg.type = TYPE_VIBRATION;
                        else if (tempPacket.type == TYPE_CURRENT) qMsg.type = TYPE_CURRENT;
                        else qMsg.type = TYPE_UNKNOWN;

                        if (qMsg.type != TYPE_UNKNOWN) {
                            memcpy(qMsg.data, tempPacket.data, sizeof(qMsg.data));
                            
                            QueueHandle_t target = (qMsg.type == TYPE_VIBRATION) ? _vibQueue : _curQueue;
                            
                            // Send to Queue with 0 wait time.
                            // If Queue is full, we DROP the packet. This prevents lag.
                            xQueueSend(target, &qMsg, 0);
                        }
                    } else {
                        // Header mismatch: We lost sync. Flush the buffer.
                        Serial.println("Sync Error: Flushing buffer...");
                        while(_clients[i].available()) _clients[i].read();
                    }
                }
            }
        }
        // Small yield to let WiFi stack process background tasks
        vTaskDelay(2); 
    }
}
