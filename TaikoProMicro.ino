//   __   ___ ___ ___ .       __   __
//  /__` |__   |   |  | |\ | / _` /__`
//  .__/ |___  |   |  | | \| \__> .__/
//
// All values below are DEFAULTS. They can be changed at runtime (and saved
// to EEPROM) from tools/InputTest.html over the controller's USB serial
// port — no re-flashing needed. "DEFAULTS" over serial restores them.

// Set to 1 to disable keyboard output and stream channel power to the
// Serial Plotter (Tools > Serial Plotter, 115200 baud) for tuning.
// (The settings page's signal monitor does the same without re-flashing.)
#define DEBUG 0

// A hit is registered when a channel's power rises above the hit
// threshold. A new hit is allowed once the hit channel's power falls back
// below RESET_THRES (raise RESET_THRES if fast rolls drop hits, lower it
// if single hits double-trigger).
#define HIT_THRES 400
#define RESET_THRES 100

// After the first channel crosses the hit threshold, keep sampling for
// this many microseconds and output the loudest channel. This rejects
// crosstalk between sensors (the strongest sensor wins, not the fastest).
#define HIT_SCAN_US 1500

// How long each key is held down, in milliseconds. Keep this at or above
// one game frame (16 ms at 60 fps) so no hit is ever missed, but below
// the fastest expected roll interval.
#define KEY_HOLD_MS 25

// Set to 1 to register simultaneous two-hand hits (big don / big kat):
// if the matching sensor on the other side also exceeds the hit threshold
// during the scan window, its key is sent as well. Set to 0 if don<->don
// or kat<->kat crosstalk on your drum causes phantom double hits.
#define DOUBLE_HITS 1

// Send an additional "special" key together with each hit (see below).
#define SPECIALS 0

// ---------------------------- Auto adjust ----------------------------
//  * Noise-floor calibration (always on): for the first AUTO_CALIB_MS
//    after plug-in the idle signal level of each sensor is measured and
//    subtracted from all readings, so electrical noise or a loose sensor
//    never causes phantom hits. Keep the drum untouched while it runs.
//  * Adaptive threshold (AUTO_ADJUST): while a hit rings, the effective
//    hit threshold rides at AUTO_THRES_RATIO percent of the live signal,
//    then decays back to the hit threshold (AUTO_THRES_DECAY percent of
//    the boost is kept per ms). Residual ringing from a hard hit can
//    never re-trigger, while soft hits still register once decayed.
#define AUTO_ADJUST 1
#define AUTO_CALIB_MS 400
#define AUTO_THRES_RATIO 150
#define AUTO_THRES_DECAY 98   // ~34 ms half-life

// Automatic sensor balancing: tracks the average hit strength of each
// zone and slowly adjusts per-channel gain so all four zones respond
// equally, within AUTO_BALANCE_MIN..MAX percent. Manual sensitivities
// still apply on top.
#define AUTO_BALANCE 1
#define AUTO_BALANCE_MIN 50
#define AUTO_BALANCE_MAX 200

// Per-channel sensitivity in percent: 100 = 1.00x, 50 = 0.50x, 150 = 1.50x.
// Use these to balance sensors that read hotter or colder than the rest.
#define L_KAT_SENS 100
#define L_KAT_KEY 'd'
#define L_KAT_KEY_SPECIAL KEY_UP_ARROW

#define L_DON_SENS 100
#define L_DON_KEY 'f'
#define L_DON_KEY_SPECIAL KEY_RETURN

#define R_DON_SENS 100
#define R_DON_KEY 'j'
#define R_DON_KEY_SPECIAL KEY_RETURN

#define R_KAT_SENS 100
#define R_KAT_KEY 'k'
#define R_KAT_KEY_SPECIAL KEY_DOWN_ARROW


//   __   __       | ___    ___  __        __         /
//  |  \ /  \ |\ |    |      |  /  \ |  | /  ` |__|  /
//  |__/ \__/ | \|    |      |  \__/ \__/ \__, |  | .
//

#include <Keyboard.h>
#include <EEPROM.h>

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
  // get(0) is the newest sample; get(1) is the oldest still stored, i.e.
  // the slot the next put() will overwrite.
  inline T get(int offset = 0) const {
    return data_[(current_ + offset) & (L - 1)];
  }

private:
  T data_[L];
  int current_ = 0;
};
#endif  // CACHE_H

#define L_KAT_IN A0
#define L_DON_IN A1
#define R_DON_IN A2
#define R_KAT_IN A3
#define CHANNELS 4
#define SAMPLE_CACHE_LENGTH 16  // must be a power of 2 (e.g. 8, 16, 32)

const byte inPins[CHANNELS] = { L_DON_IN, L_KAT_IN, R_DON_IN, R_KAT_IN };
const uint8_t outKeysSpecial[CHANNELS] = {
  L_DON_KEY_SPECIAL, L_KAT_KEY_SPECIAL, R_DON_KEY_SPECIAL, R_KAT_KEY_SPECIAL
};

// ------------------------- Runtime settings --------------------------
#define SETTINGS_MAGIC 0x54504D32UL  // "TPM2"

struct Settings {
  unsigned long magic;
  int hitThres;
  int resetThres;
  int scanUs;
  int holdMs;
  int doubleHits;
  int autoThres;
  int autoBalance;
  int sens[CHANNELS];              // percent, channel order L_DON,L_KAT,R_DON,R_KAT
  char keys[CHANNELS];
  byte checksum;
};
Settings cfg;

struct Param {
  const char* name;
  int* val;
  int lo, hi;
};
const Param params[] = {
  { "HIT",     &cfg.hitThres,    50, 20000 },
  { "RESET",   &cfg.resetThres,  10, 10000 },
  { "SCANUS",  &cfg.scanUs,     200, 10000 },
  { "HOLD",    &cfg.holdMs,       5,   200 },
  { "DOUBLE",  &cfg.doubleHits,   0,     1 },
  { "AUTO",    &cfg.autoThres,    0,     1 },
  { "BALANCE", &cfg.autoBalance,  0,     1 },
  { "SENS0",   &cfg.sens[0],     10,   400 },
  { "SENS1",   &cfg.sens[1],     10,   400 },
  { "SENS2",   &cfg.sens[2],     10,   400 },
  { "SENS3",   &cfg.sens[3],     10,   400 },
};
#define PARAM_COUNT (sizeof(params) / sizeof(params[0]))

// Sliding-window signal power per channel
Cache<long, SAMPLE_CACHE_LENGTH> inputWindow[CHANNELS];
long power[CHANNELS];
long noiseFloor[CHANNELS];        // idle level subtracted from all readings
int effGain[CHANNELS];            // sensitivity x automatic balance, percent
long dynThres;                    // adaptive hit threshold

// Hit state machine
enum HitState : byte { STATE_IDLE, STATE_SCAN, STATE_LOCKED };
HitState state = STATE_IDLE;
unsigned long scanEndUs;
long scanPeak[CHANNELS];
byte lockedChannel;

// Non-blocking key release timers
bool keyDown[CHANNELS];
char activeKey[CHANNELS];         // char actually pressed (keys can be remapped live)
unsigned long keyDownAt[CHANNELS];

unsigned long lastDecayMs;

// Auto balance state
int autoGain[CHANNELS] = { 100, 100, 100, 100 };
long emaPeak[CHANNELS];           // moving average of each zone's hit peaks
byte hitSeen[CHANNELS];           // hits recorded per zone (saturates at 255)
bool gainsDirty;

// Serial config protocol
char cmdBuf[32];
byte cmdLen;
bool monitoring;
unsigned long lastMonMs;

#if DEBUG
unsigned long lastDebugMs;
#endif

// --------------------------- Settings I/O ----------------------------
void setDefaults() {
  cfg.magic = SETTINGS_MAGIC;
  cfg.hitThres = HIT_THRES;
  cfg.resetThres = RESET_THRES;
  cfg.scanUs = HIT_SCAN_US;
  cfg.holdMs = KEY_HOLD_MS;
  cfg.doubleHits = DOUBLE_HITS;
  cfg.autoThres = AUTO_ADJUST;
  cfg.autoBalance = AUTO_BALANCE;
  const int defSens[CHANNELS] = { L_DON_SENS, L_KAT_SENS, R_DON_SENS, R_KAT_SENS };
  const char defKeys[CHANNELS] = { L_DON_KEY, L_KAT_KEY, R_DON_KEY, R_KAT_KEY };
  for (byte i = 0; i < CHANNELS; i++) {
    cfg.sens[i] = defSens[i];
    cfg.keys[i] = defKeys[i];
  }
}

byte settingsChecksum() {
  byte sum = 0;
  const byte* p = (const byte*)&cfg;
  for (unsigned int i = 0; i < offsetof(Settings, checksum); i++) sum ^= p[i];
  return sum;
}

void loadSettings() {
  EEPROM.get(0, cfg);
  if (cfg.magic != SETTINGS_MAGIC || cfg.checksum != settingsChecksum()) setDefaults();
}

void saveSettings() {
  cfg.checksum = settingsChecksum();
  EEPROM.put(0, cfg);  // put() only writes changed bytes, sparing EEPROM wear
}

// Re-derive everything that depends on the settings.
void applySettings() {
  for (byte i = 0; i < CHANNELS; i++) {
    if (!cfg.autoBalance) autoGain[i] = 100;
    effGain[i] = ((long)cfg.sens[i] * autoGain[i]) / 100;
  }
  if (!cfg.autoThres || dynThres < cfg.hitThres) dynThres = cfg.hitThres;
}

// ------------------------------ Signal -------------------------------
inline void sampleChannels() {
  // The oldest sample must be subtracted BEFORE put() overwrites it.
  for (byte i = 0; i < CHANNELS; i++) {
    long sample = ((long)analogRead(inPins[i]) * effGain[i]) / 100;
    power[i] += sample - inputWindow[i].get(1);
    inputWindow[i].put(sample);
  }
}

inline long netPower(byte i) {
  long p = power[i] - noiseFloor[i];
  return p > 0 ? p : 0;
}

// ------------------------------- Keys --------------------------------
void pressKey(byte channel) {
#if !DEBUG
  if (keyDown[channel]) {
    // Still held from the previous hit: release first so the game sees a
    // fresh key-down event.
    Keyboard.release(activeKey[channel]);
#if SPECIALS
    Keyboard.release(outKeysSpecial[channel]);
#endif
  }
  Keyboard.press(cfg.keys[channel]);
#if SPECIALS
  Keyboard.press(outKeysSpecial[channel]);
#endif
#endif
  activeKey[channel] = cfg.keys[channel];
  keyDown[channel] = true;
  keyDownAt[channel] = millis();
}

void releaseExpiredKeys() {
  unsigned long now = millis();
  for (byte i = 0; i < CHANNELS; i++) {
    if (keyDown[i] && now - keyDownAt[i] >= (unsigned long)cfg.holdMs) {
      keyDown[i] = false;
#if !DEBUG
      Keyboard.release(activeKey[i]);
#if SPECIALS
      Keyboard.release(outKeysSpecial[i]);
#endif
#endif
    }
  }
}

// --------------------------- Auto adjust -----------------------------
// Let the adaptive threshold decay back toward the hit threshold, per ms.
void decayThreshold() {
  unsigned long now = millis();
  while (lastDecayMs != now) {
    lastDecayMs++;
    long boost = dynThres - cfg.hitThres;
    if (boost <= 0) {
      dynThres = cfg.hitThres;
      lastDecayMs = now;
      break;
    }
    dynThres = cfg.hitThres + (boost * AUTO_THRES_DECAY) / 100;
  }
}

void recordHitPeak(byte channel, long peak) {
  if (hitSeen[channel] == 0) emaPeak[channel] = peak;
  else emaPeak[channel] = (emaPeak[channel] * 7 + peak) / 8;
  if (hitSeen[channel] < 255) hitSeen[channel]++;
  gainsDirty = true;
}

// Called between hits (never mid-ring, so the sliding windows stay
// consistent): nudge each zone's gain toward the all-zone average.
void updateAutoGains() {
  if (!gainsDirty) return;
  gainsDirty = false;
  long sum = 0;
  byte n = 0;
  for (byte i = 0; i < CHANNELS; i++) {
    if (hitSeen[i] >= 4) {
      sum += emaPeak[i];
      n++;
    }
  }
  if (n < 2) return;  // need at least two zones to compare
  long target = sum / n;
  for (byte i = 0; i < CHANNELS; i++) {
    if (hitSeen[i] < 4 || emaPeak[i] <= 0) continue;
    long g = (target * 100) / emaPeak[i];
    if (g < AUTO_BALANCE_MIN) g = AUTO_BALANCE_MIN;
    if (g > AUTO_BALANCE_MAX) g = AUTO_BALANCE_MAX;
    autoGain[i] = g;
    effGain[i] = ((long)cfg.sens[i] * g) / 100;
  }
}

// ------------------------ Serial config protocol ---------------------
// Line-based, used by tools/InputTest.html over Web Serial:
//   GET            -> NAME=VALUE per setting, then OK
//   SET NAME VALUE -> change a setting in RAM (applies immediately)
//   SAVE           -> persist current settings to EEPROM
//   DEFAULTS       -> restore compiled defaults (RAM only until SAVE)
//   MON 0|1        -> stream "S,lDon,lKat,rDon,rKat,thres" lines for tuning
void printSettings() {
  for (byte i = 0; i < PARAM_COUNT; i++) {
    Serial.print(params[i].name);
    Serial.print('=');
    Serial.println(*params[i].val);
  }
  for (byte i = 0; i < CHANNELS; i++) {
    Serial.print(F("KEY"));
    Serial.print(i);
    Serial.print('=');
    Serial.println(cfg.keys[i]);
  }
  Serial.println(F("OK"));
}

void handleCmd(char* line) {
  char* cmd = strtok(line, " ");
  if (!cmd) return;
  if (!strcasecmp(cmd, "GET")) {
    printSettings();
    return;
  }
  if (!strcasecmp(cmd, "SAVE")) {
    saveSettings();
    Serial.println(F("OK"));
    return;
  }
  if (!strcasecmp(cmd, "DEFAULTS")) {
    setDefaults();
    applySettings();
    Serial.println(F("OK"));
    return;
  }
  if (!strcasecmp(cmd, "MON")) {
    char* arg = strtok(NULL, " ");
    monitoring = arg && arg[0] == '1';
    Serial.println(F("OK"));
    return;
  }
  if (!strcasecmp(cmd, "SET")) {
    char* name = strtok(NULL, " ");
    char* val = strtok(NULL, " ");
    if (name && val) {
      if (!strncasecmp(name, "KEY", 3) && name[3] >= '0' && name[3] <= '3' && !name[4]) {
        cfg.keys[name[3] - '0'] = val[0];
        Serial.println(F("OK"));
        return;
      }
      for (byte i = 0; i < PARAM_COUNT; i++) {
        if (!strcasecmp(name, params[i].name)) {
          long v = atol(val);
          if (v < params[i].lo) v = params[i].lo;
          if (v > params[i].hi) v = params[i].hi;
          *params[i].val = v;
          applySettings();
          Serial.println(F("OK"));
          return;
        }
      }
    }
  }
  Serial.println(F("ERR"));
}

void pollSerial() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (cmdLen) {
        cmdBuf[cmdLen] = 0;
        handleCmd(cmdBuf);
        cmdLen = 0;
      }
    } else if (cmdLen < sizeof(cmdBuf) - 1) {
      cmdBuf[cmdLen++] = c;
    }
  }
}

// ------------------------------- Main --------------------------------
void setup() {
  Serial.begin(115200);
#if !DEBUG
  Keyboard.begin();
#endif
  analogReference(DEFAULT);
  loadSettings();
  applySettings();

  // Noise-floor calibration: measure each channel's idle level. Keep the
  // drum untouched for the first half second after plugging in.
  unsigned long calibEnd = millis() + AUTO_CALIB_MS;
  while ((long)(millis() - calibEnd) < 0) {
    sampleChannels();
    for (byte i = 0; i < CHANNELS; i++) {
      if (power[i] > noiseFloor[i]) noiseFloor[i] = power[i];
    }
  }
  // Add headroom above the observed noise.
  for (byte i = 0; i < CHANNELS; i++) noiseFloor[i] += noiseFloor[i] / 4 + 8;
  lastDecayMs = millis();
}

void loop() {
  pollSerial();
  releaseExpiredKeys();
  if (cfg.autoThres) decayThreshold();
  sampleChannels();

  switch (state) {
    case STATE_IDLE:
      for (byte i = 0; i < CHANNELS; i++) {
        if (netPower(i) >= dynThres) {
          // Something was hit: open a short scan window and record every
          // channel's peak before deciding which sensor was actually struck.
          state = STATE_SCAN;
          scanEndUs = micros() + cfg.scanUs;
          for (byte j = 0; j < CHANNELS; j++) scanPeak[j] = netPower(j);
          break;
        }
      }
      break;

    case STATE_SCAN: {
      for (byte i = 0; i < CHANNELS; i++) {
        long p = netPower(i);
        if (p > scanPeak[i]) scanPeak[i] = p;
      }
      if ((long)(micros() - scanEndUs) >= 0) {
        byte hit = 0;
        for (byte i = 1; i < CHANNELS; i++) {
          if (scanPeak[i] > scanPeak[hit]) hit = i;
        }
        pressKey(hit);
        if (cfg.autoBalance) recordHitPeak(hit, scanPeak[hit]);
        if (cfg.doubleHits) {
          // Same drum face, other hand: channel order is
          // {L_DON, L_KAT, R_DON, R_KAT}, so the partner is index ^ 2.
          byte partner = hit ^ 2;
          if (scanPeak[partner] >= dynThres) {
            pressKey(partner);
            if (cfg.autoBalance) recordHitPeak(partner, scanPeak[partner]);
          }
        }
        if (cfg.autoThres) {
          long boosted = (scanPeak[hit] * AUTO_THRES_RATIO) / 100;
          if (boosted > dynThres) dynThres = boosted;
        }
        lockedChannel = hit;
        state = STATE_LOCKED;
      }
      break;
    }

    case STATE_LOCKED:
      if (cfg.autoThres) {
        // Ride the adaptive threshold on the ringing signal: it stays at
        // AUTO_THRES_RATIO percent of the hit's live power (so it tracks
        // the true peak, not just the scan window) and decays as it fades.
        long boosted = (netPower(lockedChannel) * AUTO_THRES_RATIO) / 100;
        if (boosted > dynThres) dynThres = boosted;
      }
      if (netPower(lockedChannel) < cfg.resetThres) {
        state = STATE_IDLE;
        if (cfg.autoBalance) updateAutoGains();
      }
      break;
  }

  if (monitoring && millis() - lastMonMs >= 5) {
    lastMonMs = millis();
    Serial.print(F("S,"));
    Serial.print(netPower(0)); Serial.print(',');
    Serial.print(netPower(1)); Serial.print(',');
    Serial.print(netPower(2)); Serial.print(',');
    Serial.print(netPower(3)); Serial.print(',');
    Serial.println(dynThres);
  }

#if DEBUG
  // Stream at a rate 115200 baud can sustain without stalling sampling.
  if (millis() - lastDebugMs >= 5) {
    lastDebugMs = millis();
    Serial.print(F("L_DON:")); Serial.print(netPower(0));
    Serial.print(F("\tL_KAT:")); Serial.print(netPower(1));
    Serial.print(F("\tR_DON:")); Serial.print(netPower(2));
    Serial.print(F("\tR_KAT:")); Serial.print(netPower(3));
    Serial.print(F("\tHIT:")); Serial.print(dynThres);
    Serial.print(F("\tRESET:")); Serial.println(cfg.resetThres);
  }
#endif
}
