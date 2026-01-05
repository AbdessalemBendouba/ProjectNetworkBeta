#ifndef PROCESSING_CORE_H
#define PROCESSING_CORE_H

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <arduinoFFT.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_task_wdt.h"

class ProcessingCore{
	public:
		ProcessingCore(int eventPort = 80, const char* eventPath = "/events", int batchSamples = 256, int aggregationFactor = 4);
		void begin(QueueHandle_t vQ, QueueHandle_t cQ);
	
	private:
		int			_aggregationFactor;
		int			_batchSamples;
		int			_fftPools;
		
		float*		_latestVibData;
		float*		_latestCurData;
		float*		_latestVibFft;
		float*		_latestCurFft;
		
		const char*	_eventPath;
		int			_eventPort;
		
		AsyncWebServer _webServer;
		AsyncEventSource _events; 
		
        QueueHandle_t _vibQueue;
        QueueHandle_t _curQueue;
		
		float*		_vReal;
		float*		_vImag;
		ArduinoFFT<float>* _FFT = nullptr; 
		
		int _vibCount = 0;
		int _curCount = 0;
		
		void processingWorker();
		static void taskWrapper(void* pvParameters);

};
#endif