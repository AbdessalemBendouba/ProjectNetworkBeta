#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <Arduino.h>

#define BATCH_SAMPLES 256
#define PACKET_HEADER 0xA5A5

enum SensorDataType {
    TYPE_UNKNOWN = 0, 
    TYPE_VIBRATION = 1, 
    TYPE_CURRENT = 2 
};

// The exact binary structure sent over WiFi (1028 bytes total)
typedef struct {
    uint16_t header;         // Safety check (0xA5A5)
    int16_t  type;           // 1 = Vibration, 2 = Current
    float    data[BATCH_SAMPLES]; 
} DataPacket_t;

typedef struct {
    SensorDataType type;
    float data[BATCH_SAMPLES];
} InternalMessage_t;

#endif