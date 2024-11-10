//   __   ___ ___ ___ .       __   __  
//  /__` |__   |   |  | |\ | / _` /__` 
//  .__/ |___  |   |  | | \| \__> .__/ 
//                                                                                      

#define HIT_THRES 400
#define RESET_THRES 100
#define SPECIALS 0

#define L_KAT_SENS 1
#define L_KAT_KEY 'd'
#define L_KAT_KEY_SPECIAL 218 // UP ARROW

#define L_DON_SENS 1
#define L_DON_KEY 'f'
#define L_DON_KEY_SPECIAL 176 // ENTER KEY

#define R_DON_SENS 1
#define R_DON_KEY 'j'
#define R_DON_KEY_SPECIAL 176 // ENTER KEY

#define R_KAT_SENS 1
#define R_KAT_KEY 'k'
#define R_KAT_KEY_SPECIAL 217 // DOWN ARROW






//   __   __       | ___    ___  __        __         /
//  |  \ /  \ |\ |    |      |  /  \ |  | /  ` |__|  / 
//  |__/ \__/ | \|    |      |  \__/ \__/ \__, |  | .  
//

#ifndef CACHE_H
#define CACHE_H
template <class T, int L>
class Cache {
public:
  Cache() { memset(data_, 0, sizeof(data_)); }
  inline void put(T value) {
    current_ = (current_ + 1) & (L - 1);
    data_[current_] = value;
  }
  inline T get(int offset = 0) const {
    return data_[(current_ + offset) & (L - 1)];
  }

private:
  T data_[L];
  int current_ = 0;
};
#endif  // CACHE_H

#include <Keyboard.h>
#include <limits.h>
//#include "cache.h"
#define L_KAT_IN A0
#define L_DON_IN A1
#define R_DON_IN A2
#define R_KAT_IN A3
#define CHANNELS 4
#define SAMPLE_CACHE_LENGTH 16 // SAMPLE_CACHE_LENGTH must be power of 2 (e.g., 8, 16, 32, etc.)

// Cache & Power Variables
Cache < int, SAMPLE_CACHE_LENGTH > inputWindow[CHANNELS];
unsigned long power[CHANNELS];
unsigned long lastPower[CHANNELS];

// Trigger state
bool triggered;
unsigned long triggeredTime[CHANNELS];

// Input pins array
const byte inPins[] = {
  L_DON_IN,
  L_KAT_IN,
  R_DON_IN,
  R_KAT_IN
};

// Output keys array
const char outKeys[] = {
  L_DON_KEY,
  L_KAT_KEY,
  R_DON_KEY,
  R_KAT_KEY
};

const char outKeysSpecial[] = {
  L_DON_KEY_SPECIAL,
  L_KAT_KEY_SPECIAL,
  R_DON_KEY_SPECIAL,
  R_KAT_KEY_SPECIAL
};

// Sensitivities array (now integers)
int sensitivities[] = {
  L_DON_SENS,
  L_KAT_SENS,
  R_DON_SENS,
  R_KAT_SENS
};

// Max power tracking variables
short maxIndex;
float maxPower;

void setup() {
  Serial.begin(115200);
  Keyboard.begin();
  analogReference(DEFAULT);

  // Initialize power and lastPower arrays
  for (byte i = 0; i < CHANNELS; i++) {
    power[i] = 0;
    lastPower[i] = 0;
    triggered = false;
  }

  maxIndex = -1;
  maxPower = 0;
}

void loop() {
  // Reset trigger if the max power drops below the reset threshold
  if (maxIndex != -1 && lastPower[maxIndex] < RESET_THRES) {
    triggered = false;
    maxIndex = -1; // Reset max index
    maxPower = 0; // Reset max power
  }
  // Loop through each channel
  for (byte i = 0; i < CHANNELS; i++) {
    // Update the input window and power
    inputWindow[i].put(analogRead(inPins[i]));
    power[i] = ((999 + sensitivities[i]) * (power[i] - inputWindow[i].get(1) + inputWindow[i].get())) / 1000;
    // Track the maximum power and index
    if (lastPower[i] > maxPower && power[i] < lastPower[i]) {
      maxPower = lastPower[i];
      maxIndex = i;
    }
    lastPower[i] = power[i];
  }
  // Trigger action if the max power exceeds the threshold
  if (!triggered && maxPower >= HIT_THRES) {
    triggered = true;
    Keyboard.print(outKeys[maxIndex]);
    #if SPECIALS
      Keyboard.print(outKeysSpecial[maxIndex]);
    #endif
  }
}