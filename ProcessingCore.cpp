#include <Arduino.h>
#include "ProcessingCore.h"
#include "WebCode.h"
#include "Protocol.h"

ProcessingCore::ProcessingCore(int eventPort, const char* eventPath, int batchSamples, int aggregationFactor) : _webServer(eventPort), _events(eventPath), _batchSamples(batchSamples), _aggregationFactor(aggregationFactor){
	
	
	_fftPools = aggregationFactor * batchSamples;
	
	_latestVibData	= new float[_fftPools];
	_latestCurData	= new float[_fftPools];
	_latestVibFft	= new float[_fftPools / 2];
	_latestCurFft	= new float[_fftPools / 2];
	
	_vReal	= new float[_fftPools];
	_vImag	= new float[_fftPools];
	_FFT	= new ArduinoFFT<float> (_vReal, _vImag, _fftPools, 1000.0, false);
}

void ProcessingCore::begin(QueueHandle_t vQ, QueueHandle_t cQ){
	_vibQueue = vQ; _curQueue = cQ;
	
    _webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *req){ 
        req->send_P(200, "text/html", index_html);
    });
    _events.onConnect([](AsyncEventSourceClient *client){
        client->send("Connected", NULL, millis(), 1000);
    });
    _webServer.addHandler(&_events);
    _webServer.begin();
	
	xTaskCreatePinnedToCore(
        taskWrapper,    // Function to run
        "ProcTask",     // Name
        30000,           // Stack size
        this,           // PASSING THE INSTANCE
        1,              // Priority
        NULL,           // Task handle
        1               // Core 1
    );
}

void ProcessingCore::taskWrapper(void* pvParameters){
	ProcessingCore* instance = (ProcessingCore*) pvParameters; 
    instance->processingWorker();
}

void ProcessingCore::processingWorker(){
    InternalMessage_t incoming;
    static char jsonBuffer[8192]; // Large buffer for JSON string
    
    // Throttling variables to prevent flooding the web browser
    unsigned long lastWebUpdate = 0;
    const unsigned long WEB_UPDATE_INTERVAL = 200; // 200ms = 5 Updates per second max

    for(;;) {
        // -------------------------
        // --- PROCESS VIBRATION ---
        // -------------------------
        if (xQueueReceive(_vibQueue, &incoming, 0) == pdPASS) {
            
            // 1. Copy data into the large FFT buffer
            memcpy(&_latestVibData[_vibCount * _batchSamples], incoming.data, sizeof(float)*_batchSamples);
            _vibCount++;

            // 2. If we have 4 batches (1024 samples), run FFT
            if (_vibCount == _aggregationFactor) {
                
                // Prepare FFT arrays
                memcpy(_vReal, _latestVibData, sizeof(_vReal));
                memset(_vImag, 0, sizeof(_vImag));
                
                // Execute FFT
                _FFT->dcRemoval();
                _FFT->windowing(FFTWindow::Hann, FFTDirection::Forward, false);
                _FFT->compute(FFTDirection::Forward);
                _FFT->complexToMagnitude();
                
                // Store Result
                memcpy(_latestVibFft, _vReal, sizeof(_latestVibFft));

                // 3. Send to Web (Throttled)
                if (millis() - lastWebUpdate > WEB_UPDATE_INTERVAL) {
                    
                    // Manually build JSON string for speed
                    int len = sprintf(jsonBuffer, "{\"type\":\"vib\",\"fft\":[");
                    
                    // Add 512 FFT points
                    for (int i = 0; i < 512; i++) {
                        len += sprintf(jsonBuffer + len, "%.2f%s", _latestVibFft[i], (i<511)?",":"]");
                    }
                    
                    // Add Time Domain Data
                    // IMPORTANT: We skip every 4th sample to save bandwidth (1024 -> 256 points)
                    len += sprintf(jsonBuffer + len, ",\"time\":[");
                    for (int i = 0; i < 1024; i+=4) { 
                        len += sprintf(jsonBuffer + len, "%.2f%s", _latestVibData[i], (i<1020)?",":"]}");
                    }

                    // Send via SSE
                    _events.send(jsonBuffer, "update", millis());
                    lastWebUpdate = millis();
                }
                _vibCount = 0; // Reset counter
            }
        }
        
        // -----------------------
        // --- PROCESS CURRENT ---
        // -----------------------
        if (xQueueReceive(_curQueue, &incoming, 0) == pdPASS) {
            
            // 1. Copy data
            memcpy(&_latestCurData[_curCount * _batchSamples], incoming.data, sizeof(float)*_batchSamples);
            _curCount++;

            // 2. If aggregated, run FFT
            if (_curCount == _aggregationFactor) {
                memcpy(_vReal, _latestCurData, sizeof(_vReal));
                memset(_vImag, 0, sizeof(_vImag));
                _FFT->dcRemoval();
                _FFT->windowing(FFTWindow::Hann, FFTDirection::Forward, false);
                _FFT->compute(FFTDirection::Forward);
                _FFT->complexToMagnitude();
                memcpy(_latestCurFft, _vReal, sizeof(_latestCurFft));

                // 3. Send to Web (Throttled)
                if (millis() - lastWebUpdate > WEB_UPDATE_INTERVAL) {
                    
                    int len = sprintf(jsonBuffer, "{\"type\":\"cur\",\"fft\":[");
                    for (int i = 0; i < 512; i++) {
                        len += sprintf(jsonBuffer + len, "%.2f%s", _latestCurFft[i], (i<511)?",":"]");
                    }
                    len += sprintf(jsonBuffer + len, ",\"time\":[");
                    for (int i = 0; i < 1024; i+=4) { 
                        len += sprintf(jsonBuffer + len, "%.2f%s", _latestCurData[i], (i<1020)?",":"]}");
                    }

                    _events.send(jsonBuffer, "update", millis());
                    lastWebUpdate = millis();
                }
                _curCount = 0;
            }
        }

        // Small yield to allow other tasks to run if needed
        vTaskDelay(5); 
    }
}
