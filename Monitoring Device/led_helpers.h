#include "InternetButton.h"
#include "math.h"


extern InternetButton b;

struct NotifyState {
    int ledNum = -1;
    uint8_t red = 0, green = 0, blue = 0;
    unsigned long startTime = 0;
    unsigned long duration = 0;
    bool active = false;
};

NotifyState notifyPulse;
const unsigned long defaultNotifyDuration = 250;


void notify(int ledNum, uint8_t r, uint8_t g, uint8_t b, unsigned long durationMs = defaultNotifyDuration) {
    notifyPulse.ledNum = ledNum;
    notifyPulse.red = r;
    notifyPulse.green = g;
    notifyPulse.blue = b;
    notifyPulse.duration = durationMs;
    notifyPulse.startTime = millis();
    notifyPulse.active = true;
}

void updateNotifyPulse() {
    if (!notifyPulse.active) return;

    unsigned long now = millis();
    if (now - notifyPulse.startTime < notifyPulse.duration) {
        b.ledOn(notifyPulse.ledNum,
               notifyPulse.red,
               notifyPulse.green,
               notifyPulse.blue);
    } else {
        notifyPulse.active = false;
        // No need to manually turn off—the breathing will resume in the next update cycle
    }
}

// use: notify(6,255,0,0, 250); // LED 6 blink quickly to notify and goes off again

struct BlinkLED {
  uint8_t pin;
  uint32_t lastToggle = 0;
  bool state = false;
  bool active = false;  // NEW: controls whether blinking runs
  uint32_t interval;
  uint8_t r, g, b;
};

BlinkLED statusLed;

void startBlink(uint8_t i, uint8_t r, uint8_t g, uint8_t b, uint32_t speedMs) {
  statusLed = {i, millis() - speedMs, false, true, speedMs, r, g, b};  // active = true
}


void updateBlink(InternetButton& b) {
  if (!statusLed.active) return;  // skip if inactive

  uint32_t now = millis();
  if (now - statusLed.lastToggle >= statusLed.interval) {
    statusLed.lastToggle = now;
    statusLed.state = !statusLed.state;

    if (statusLed.state) {
      b.ledOn(statusLed.pin, statusLed.r, statusLed.g, statusLed.b);
    } else {
      b.ledOff(statusLed.pin);
    }
  }
}



// startBlink(3, 255, 0, 0, 500);  // Blink LED at index 3 red every 500ms


struct LedTrickleState {
    int ledStart;       // first LED in the trickle range
    int ledEnd;         // last LED in the trickle range
    bool directionDown; // true = start to end, false = end to start
    unsigned long lastUpdate = 0;
    int currentStep = 0;
    int intervalMs = 100;
    uint8_t red, green, blue;
    bool active = false;
};

LedTrickleState trickle;


void startTrickle(int startLED, int endLED, bool directionDown, uint8_t red, uint8_t green, uint8_t blue, int intervalMs = 100) {
    trickle.ledStart = startLED;
    trickle.ledEnd = endLED;
    trickle.directionDown = directionDown;
    trickle.currentStep = 0;
    trickle.intervalMs = intervalMs;
    trickle.lastUpdate = millis();
    trickle.red = red;
    trickle.green = green;
    trickle.blue = blue;
    trickle.active = true;
}

void updateTrickleEffect() {
    if (!trickle.active) return;

    unsigned long now = millis();
    if (now - trickle.lastUpdate < trickle.intervalMs) return;

    trickle.lastUpdate = now;

    // Clear previous LED
    int prevIdx = trickle.directionDown
        ? trickle.ledStart + trickle.currentStep - 1
        : trickle.ledEnd - trickle.currentStep + 1;

    if (prevIdx >= trickle.ledStart && prevIdx <= trickle.ledEnd) {
        b.ledOn(prevIdx, 0, 0, 0); // turn off
    }

    // Determine current LED index
    int ledIdx = trickle.directionDown
        ? trickle.ledStart + trickle.currentStep
        : trickle.ledEnd - trickle.currentStep;

    // Light current LED
    if (ledIdx >= trickle.ledStart && ledIdx <= trickle.ledEnd) {
        b.ledOn(ledIdx, trickle.red, trickle.green, trickle.blue);
    }

    trickle.currentStep++;

    // Loop around when reaching the end
    if (trickle.currentStep > (trickle.ledEnd - trickle.ledStart)) {
        trickle.currentStep = 0;
    }
}


// startTrickle(6, 10, true, 255, 80, 0, 120); // orange trickle from LED 6 to 10
// startTrickle(6, 10, false, 255, 80, 0, 120); // reversed direction


enum BreathSpeed { BLINK, FAST, MEDIUM, SLOW };

unsigned long getBreathDelay(BreathSpeed speed) {
    switch (speed) {
        case BLINK: return 10;        
        case FAST: return 20;
        case MEDIUM: return 40;
        case SLOW: return 80;  // original delay
        default: return 80;
    }
}


struct LedBreathState {
    int ledNum;
    uint8_t red, green, blue;
    unsigned long lastUpdate = 0;
    int step = 0;
    bool active = false;
    BreathSpeed speed = MEDIUM;    
};


const int breathSteps = 100; //200;
int breathDelayMs = 80; // 30;
LedBreathState breath;
const float pi = 3.14159;

LedBreathState statusBreath; // for LED 9 or any non-ripple solo LED

const int maxBreathLEDs = 11; // or however many LEDs you may animate
LedBreathState breaths[maxBreathLEDs];
int breathCount = 0;

void startArrayBreath(int ledNum, uint8_t red, uint8_t green, uint8_t blue, BreathSpeed speed = SLOW) {
    if (breathCount >= maxBreathLEDs) return;
    breaths[breathCount] = {ledNum, red, green, blue, millis(), 0, true, speed};  // Includes speed
    breathCount++;
}

// use: addBreathGroup(1, 6, 255, 0, 0, SLOW); // LEDs 1–6 red breathing slowly

void startStatusBreath(int ledNum, uint8_t red, uint8_t green, uint8_t blue, BreathSpeed speed = MEDIUM) {
    statusBreath = {ledNum, red, green, blue, millis(), 0, true, speed};  // Includes speed
}

// use: startStatusBreath(7, 255,255,255,BLINK); // lED 7 white breathing fast


void updateBreathingLEDs() {
    unsigned long now = millis();

    // Dynamic breath group update
    for (int i = 0; i < breathCount; i++) {
        if (!breaths[i].active) continue;
        if (notifyPulse.active && breaths[i].ledNum == notifyPulse.ledNum) continue;

        unsigned long delayMs = getBreathDelay(breaths[i].speed);  
        if (now - breaths[i].lastUpdate < delayMs) continue;

        breaths[i].lastUpdate = now;

        float progress = (float)breaths[i].step / breathSteps;
        float brightness = pow((sin(progress * pi)), 2);  // smooth breathing

        b.ledOn(breaths[i].ledNum,
                (uint8_t)(breaths[i].red * brightness),
                (uint8_t)(breaths[i].green * brightness),
                (uint8_t)(breaths[i].blue * brightness));

        breaths[i].step++;
        if (breaths[i].step > breathSteps) {
            breaths[i].step = 0;
        }
    }

    // Standalone status LED update
    if (statusBreath.active) {
        unsigned long statusDelayMs = getBreathDelay(statusBreath.speed);  
        
        if (now - statusBreath.lastUpdate >= statusDelayMs) {
            statusBreath.lastUpdate = now;
            float progress = (float)statusBreath.step / breathSteps;
            float brightness = pow((sin(progress * pi)), 2);

            b.ledOn(statusBreath.ledNum,
                    (uint8_t)(statusBreath.red * brightness),
                    (uint8_t)(statusBreath.green * brightness),
                    (uint8_t)(statusBreath.blue * brightness));

            statusBreath.step++;
            if (statusBreath.step > breathSteps) {
                statusBreath.step = 0;
            }
        }
    }
}


void addBreathGroup(int startLED, int count, uint8_t r, uint8_t g, uint8_t b, BreathSpeed speed = SLOW) {
    for (int i = 0; i < count && breathCount < maxBreathLEDs; i++) {
        breaths[breathCount++] = {startLED + i, r, g, b, millis(), 0, true, speed};
    }
}


void stopStatusBreath() {
    statusBreath.active = false;
    b.ledOff(statusBreath.ledNum);
}

void stopBreathGroup() {
    for (int i = 0; i < breathCount; i++) {
        breaths[i].active = false;
        b.ledOff(breaths[i].ledNum); // individual cleanup
    }
    breathCount = 0;
}


void stopTrickle() {
    trickle.active = false;
    trickle.currentStep = 0;
    for (int i = trickle.ledStart; i <= trickle.ledEnd; i++) {
        b.ledOff(i);
    }
}

void stopNotifyPulse() {
    notifyPulse.active = false;
    b.ledOff(notifyPulse.ledNum);
}


void stopBlink() {
  b.ledOff(statusLed.pin);
  statusLed.active = false;
  statusLed.state = false;
}



void clearAllEffects() {
    stopBlink();
    stopNotifyPulse();
    stopTrickle();
    stopBreathGroup();
    stopStatusBreath();
}

