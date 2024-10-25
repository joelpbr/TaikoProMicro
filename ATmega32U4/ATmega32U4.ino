#define CHANNELS 4

// SAMPLE_CACHE_LENGTH must be power of 2 (8, 16, 32, etc.)
#define SAMPLE_CACHE_LENGTH 16

// Thresholds for hit detection
#define HIT_THRES 300
#define RESET_THRES 200

// Sensitivity multipliers for each channel
#define L_DON_SENS 1.0
#define L_KAT_SENS 1.0
#define R_DON_SENS 1.0
#define R_KAT_SENS 0.9999

// Input pins for each channel
#define L_DON_IN A1
#define L_KAT_IN A0
#define R_DON_IN A2
#define R_KAT_IN A3

// Keyboard output for each channel
#define L_DON_KEY 'f'
#define L_KAT_KEY 'd'
#define R_DON_KEY 'j'
#define R_KAT_KEY 'k'

// Enable debug mode to view analog input values in Serial Monitor
#define DEBUG 0

#include <Keyboard.h>
#include <limits.h>
#include "cache.h"

Cache<int, SAMPLE_CACHE_LENGTH> inputWindow[CHANNELS];
unsigned long power[CHANNELS];
unsigned long lastPower[CHANNELS];

bool triggered[2];  // Separate triggered flags for left and right sides
const byte inPins[] = {L_DON_IN, L_KAT_IN, R_DON_IN, R_KAT_IN};
const char outKeys[] = {L_DON_KEY, L_KAT_KEY, R_DON_KEY, R_KAT_KEY};
float sensitivities[] = {L_DON_SENS, L_KAT_SENS, R_DON_SENS, R_KAT_SENS};

short maxIndex[2];
float maxPower[2];

void setup() {
    Serial.begin(115200);
    Keyboard.begin();
    analogReference(DEFAULT);
    for (byte i = 0; i < CHANNELS; i++) {
        power[i] = 0;
        lastPower[i] = 0;
    }
    triggered[0] = triggered[1] = false;
    maxIndex[0] = maxIndex[1] = -1;
    maxPower[0] = maxPower[1] = 0;
}

void loop() {
    for (byte side = 0; side < 2; side++) {
        if (maxIndex[side] != -1 && lastPower[maxIndex[side]] < RESET_THRES) {
            triggered[side] = false;
            maxIndex[side] = -1;
            maxPower[side] = 0;
        }
    }

    for (byte i = 0; i < CHANNELS; i++) {
        inputWindow[i].put(analogRead(inPins[i]));
        power[i] = sensitivities[i] *
                   (power[i] - inputWindow[i].get(1) + inputWindow[i].get());

        byte side = i < 2 ? 0 : 1;  // Left side (0) or right side (1)

        // Check if the other channel on this side has higher power
        byte otherChannel = (i % 2 == 0) ? i + 1 : i - 1;
        if (power[i] > power[otherChannel] + HIT_THRES / 2) {
            power[otherChannel] = 0;  // Suppress adjacent channel's power
        }

        // Update max power for this side if current power is a new maximum
        if (lastPower[i] > maxPower[side] && power[i] < lastPower[i]) {
            maxPower[side] = lastPower[i];
            maxIndex[side] = i;
        }
        lastPower[i] = power[i];

#if DEBUG
        Serial.print(power[i]);
        Serial.print(" ");
#endif
    }

    for (byte side = 0; side < 2; side++) {
        if (!triggered[side] && maxPower[side] >= HIT_THRES) {
            triggered[side] = true;
#if !DEBUG
            Keyboard.print(outKeys[maxIndex[side]]);
#endif
        }
    }

#if DEBUG
    Serial.print("\n");
#endif
}
