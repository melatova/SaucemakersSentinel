/*
 * Project:     Saucier’s Sentinel - Hot Process Timer
 * Author:      Nicole
 * Date:        03/07/2026
 *
 * Description:
 *   IoT temperature monitor for a 10-minute hot-process cycle.
 *   Tracks sauce temperature with a K-type thermocouple and alerts the
 *   chef via OLED display, Philips Hue bulbs, and Wemo smart plugs when
 *   the temperature drifts outside the safe range (185-200 degF).
 *   Also calculates the local boiling point from live air pressure —
 *   handy in Albuquerque, where water boils around 202 degF.
 *
 * Hardware pins:
 *   D7  — Power indicator LED (solid=idle, slow tick=timing, fast=done)
 *   D6  — Start/Cancel button (short press=start, hold 2s=cancel)
 *   D5  — Thermocouple CS   (MAX6675 chip select)
 *   D4  — Thermocouple SCK  (MAX6675 clock)
 *   D3  — Thermocouple MISO (MAX6675 data out)
 *   D1  — SCL: shared by OLED (SSD1306) and environment sensor (BME280)
 *   D0  — SDA: shared by OLED (SSD1306) and environment sensor (BME280)
 *   EN  — Hardware power switch
 *
 * Loop order every frame:
 *   readSensors → updateButtons → runStateMachine
 *   → updateAlerts → updateLED → updateDisplay
 *
 * Can change CYCLE_MS is set to 10000 (10 sec) for testing.
 *   Change to 600000 for the real 10-minute hot-process cycle.
 */


// ════════════════════════════════════════════════════════════════
//  LIBRARIES
// ════════════════════════════════════════════════════════════════

#include "Particle.h"
#include <math.h>              // logf() — used in boiling point calculation
#include "MAX6675.h"           // K-type thermocouple reader
#include "Adafruit_BME280.h"   // temperature / pressure / humidity sensor
#include "Adafruit_GFX.h"      // graphics resource (text, shapes)
#include "Adafruit_SSD1306.h"  // OLED display driver
#include "IoTTimer.h"          // non-blocking timers (no delay())
#include "bitmaps.h"           // original pixel art icons
#include "ui_theme.h"          // layout constants, font sizes, thresholds
#include "wemo.h"              // Wemo smart plug control
#include "hue.h"               // Philips Hue bulb control

SYSTEM_MODE(SEMI_AUTOMATIC);   // Call WiFi.connect() in setup()
SYSTEM_THREAD(ENABLED);        // keeps sensors and display responsive even if WiFi is slow


// ════════════════════════════════════════════════════════════════
//  DATA TYPES
// ════════════════════════════════════════════════════════════════

// The seven screens the device can be on at any moment.
// The state machine in runStateMachine() moves between these.
enum ScreenState {
    SCREEN_LOADING,            // loading screen while device warms up
    SCREEN_TEMP_MONITOR,       // idle — showing live temp, waiting to start
    SCREEN_CYCLE_IN_PROGRESS,  // timer running, temp in safe range
    SCREEN_ALERT_LOW,          // timer running, temp too cold
    SCREEN_ALERT_HIGH,         // timer running, temp too hot
    SCREEN_CANCELLING,         // chef held button — counting down to cancel
    SCREEN_SUCCESS,             // 10 minutes complete — safe to bottle!
    SCREEN_TIMER_SUCCESS        // 10 minutes complete! Just a kitchen timer.
};


// ════════════════════════════════════════════════════════════════
//  OBJECTS
// ════════════════════════════════════════════════════════════════

MAX6675          thermocouple;           // reads the probe in the pot
Adafruit_BME280  BME;                    // reads room pressure + humidity
Adafruit_SSD1306 display(-1);           // -1 = no hardware reset pin


// ════════════════════════════════════════════════════════════════
//  GLOBALS — constants
// ════════════════════════════════════════════════════════════════

// ── Hardware pins ────────────────────────────────────────────────
const int POWER_LEDPIN = D7;   // status LED
const int BUTTON_PIN   = D6;   // start/cancel button (INPUT_PULLUP: LOW=pressed)
const int BME_ADDRESS  = 0x76; // I2C address of BME280

// ── Hue bulbs and Wemo plugs ─────────────────────────────────────
const int BULB1     = 1;   // Hue bulb 1
const int BULB2     = 2;   // Hue bulb 2
const int WEMO_STOVE = 3;  // Wemo controlling the stove
const int WEMO_RADIO = 4;  // Wemo controlling the kitchen radio

// ── Button timing ────────────────────────────────────────────────
const unsigned long LONG_PRESS_MS = 2000;  // hold this long to cancel
const unsigned long DEBOUNCE_MS   =   20;  // ignore bounces shorter than this

// ── Cycle and screen timers ──────────────────────────────────────
// CYCLE_MS: how long the hot-process timer runs.
// Set to 10000 (10 sec) for testing, 600000 (10 min) for real hot-process.
const unsigned long LOADING_MS   =  7000;  // loading screen duration
const unsigned long CYCLE_MS     = 600000;  // change to 10000 for testing
const unsigned long CANCEL_MS    =  5000;  // cancelling animation duration
const unsigned long TEMP_READ_MS =   500;  // read sensors every 500ms


// ════════════════════════════════════════════════════════════════
//  GLOBALS — variables
// ════════════════════════════════════════════════════════════════

// ── Sensor readings ──────────────────────────────────────────────
float tempC    = 0.0;   // thermocouple reading in Celsius
float tempF    = 0.0;   // thermocouple reading in Fahrenheit
float pressPA  = 0.0;   // air pressure from BME280, raw Pascals
float humidRH  = 0.0;   // relative humidity from BME280 (reserved for future use)
float boilingF = 0.0;   // calculated local boiling point in degF

// ── State machine ────────────────────────────────────────────────
ScreenState currentScreen  = SCREEN_LOADING;
ScreenState previousScreen = SCREEN_LOADING;  // lets us detect screen transitions

// ── Cycle timing ─────────────────────────────────────────────────
unsigned long cycleStartTime = 0;   // millis() when chef pressed start
unsigned long cycleEndTime   = 0;   // millis() when cycle completed

// ── Button state ─────────────────────────────────────────────────
// Read the button directly with digitalRead() rather than the Button
// library — the library's debounce was causing bugs.
bool          buttonHeld              = false;
bool          buttonShortPress        = false;
bool          buttonReleasedAfterStart = false;  // prevents instant-cancel on start
unsigned long buttonPressStart        = 0;
bool          lastRawState            = HIGH;    // HIGH = not pressed (INPUT_PULLUP)
unsigned long lastDebounceTime        = 0;

// ── Alert state ──────────────────────────────────────────────────
bool alertsActive = false;   // true while Hue/Wemo are in alert state


// ════════════════════════════════════════════════════════════════
//  TIMERS
// ════════════════════════════════════════════════════════════════

IoTTimer loadingTimer;    // times the splash screen
IoTTimer cycleTimer;      // counts down the hot-process cycle
IoTTimer cancelTimer;     // counts down the cancelling animation
IoTTimer tempReadTimer;   // paces the sensor reads


// ════════════════════════════════════════════════════════════════
//  FUNCTION DECLARATIONS (prototypes)
// ════════════════════════════════════════════════════════════════
// Particle resolves these automatically, but listing them here
// matches the style guide and makes the structure easy to see at a glance.

void   initSensors();
float  calcBoilingPointC(float pressPA);
void   readThermocouple();
void   readBME();
void   updateButtons();
bool   wasShortPress();
bool   wasCancelHeld();
void   initTimers();
void   runStateMachine();
void   clearAlerts();
void   updateAlerts();
void   initDisplay();
void   drawTempBanner();
void   drawSeparator();
uint8_t animFrame(unsigned long speedMs, uint8_t numFrames);
void   displayLoadingScreen();
void   displayTempScreen();
void   displayCycleScreen();
void   displayLowAlertScreen();
void   displayHighAlertScreen();
void   displayCancellingScreen();
void   displaySuccessScreen();
void   displayTimerSuccessScreen();
void   updateLED();


// ════════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════════

void setup() {
    Serial.begin(9600);
    waitFor(Serial.isConnected, 10000);

    pinMode(POWER_LEDPIN, OUTPUT);
    digitalWrite(POWER_LEDPIN, HIGH);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    initSensors();
    initTimers();
    initDisplay();

    currentScreen  = SCREEN_LOADING;
    previousScreen = SCREEN_LOADING;

    WiFi.on();
    WiFi.setCredentials("IoTNetwork");
    WiFi.connect();
}


// ════════════════════════════════════════════════════════════════
//  LOOP
// ════════════════════════════════════════════════════════════════

void loop() {

    // Read sensors every 500ms — no need for faster than that
    if (tempReadTimer.isTimerReady()) {
        readThermocouple();
        readBME();
        tempReadTimer.startTimer(TEMP_READ_MS);
    }

    updateButtons();      // check button, set button state
    runStateMachine();    // decide which screen we should be on
    updateAlerts();       // fire Hue/Wemo only on state transitions
    updateLED();          // blink the D7 LED to match current state

    // Draw whichever screen the state machine landed on
    switch (currentScreen) {
        case SCREEN_LOADING:           displayLoadingScreen();    break;
        case SCREEN_TEMP_MONITOR:      displayTempScreen();       break;
        case SCREEN_CYCLE_IN_PROGRESS: displayCycleScreen();      break;
        case SCREEN_ALERT_LOW:         displayLowAlertScreen();   break;
        case SCREEN_ALERT_HIGH:        displayHighAlertScreen();  break;
        case SCREEN_CANCELLING:        displayCancellingScreen(); break;
        case SCREEN_SUCCESS:           displaySuccessScreen();    break;
        case SCREEN_TIMER_SUCCESS:     displayTimerSuccessScreen(); break;
    }
}


// ════════════════════════════════════════════════════════════════
//  SENSOR FUNCTIONS
// ════════════════════════════════════════════════════════════════

// Start up the thermocouple and BME280 — must be called in setup().
void initSensors() {
    thermocouple.begin(D4, D5, D3);   // SCK=D4, CS=D5, MISO=D3
    BME.begin(BME_ADDRESS);
}

// Calculates the local boiling point of water from air pressure.
// This uses the Clausius-Clapeyron equation — the physics behind why
// pasta takes longer to cook in Denver (or Albuquerque!).
//
// Water boils when its own vapor pressure equals the surrounding air pressure.
// Less air pushing down = water escapes as steam at a lower temperature.
// At sea level (1013.25 hPa): returns 100 degC exactly.
// In Albuquerque (~834 hPa):  returns ~94-95 degC (~202 degF).
//
// pressPA: raw BME280 pressure reading in Pascals
// returns: boiling point in Celsius
float calcBoilingPointC(float pressPA) {
    float pressHPA = pressPA / 100.0;    // Pascals to hectopascals (hPa)
    float L        = 40650.0;            // latent heat of vaporization, water (J/mol)
    float R        =  8.314;             // universal gas constant (J/mol/K)
    float T0       = 373.15;             // sea-level boiling point in Kelvin (100 degC)
    float P0       = 1013.25;            // sea-level reference pressure (hPa)

    // Clausius-Clapeyron rearranged: 1/T = 1/T0 - (R/L) * ln(P/P0)
    float invT = (1.0 / T0) - (logf(pressHPA / P0) * R / L);
    return (1.0 / invT) - 273.15;       // convert Kelvin back to Celsius
}

// Reads the K-type thermocouple and converts to both Fahrenheit and Celsius.
void readThermocouple() {
    thermocouple.read();
    tempC = thermocouple.getTemperature();
    tempF = (9.0 / 5.0) * tempC + 32.0;
}

// Reads pressure and humidity from the BME280, then recalculates
// the local boiling point. Updates every 500ms, so even a passing
// storm front will shift the boiling point display slightly.
void readBME() {
    pressPA = BME.readPressure();   // raw Pascals
    humidRH = BME.readHumidity();   // % RH (reserved for future display)

    float boilC = calcBoilingPointC(pressPA);
    boilingF = boilC * 9.0 / 5.0 + 32.0;
}


// ════════════════════════════════════════════════════════════════
//  BUTTON FUNCTIONS
// ════════════════════════════════════════════════════════════════
//
// Using raw digitalRead() with manual debounce instead of the Button
// library. Debugging proved that this works better for this project.
//
// The button is wired INPUT_PULLUP:
//   HIGH = not pressed (the pull-up holds the pin high)
//   LOW  = pressed     (button connects pin to ground)
//
// On each loop frame:
//   - If the raw signal changed, reset the debounce timer.
//   - If the signal has been stable for DEBOUNCE_MS (20ms), treat it as real.
//   - Falling edge (HIGH to LOW): record when the press started.
//   - Rising edge  (LOW to HIGH): if held was short → note a short press.

// Reads the button each frame and updates the button state.
void updateButtons() {
    bool rawState = (digitalRead(BUTTON_PIN) == LOW);   // true = pressed

    // Reset the debounce clock whenever the raw signal changes
    if (rawState != lastRawState) {
        lastDebounceTime = millis();
        lastRawState     = rawState;
    }

    // Wait until the signal has settled before acting on it
    if ((millis() - lastDebounceTime) < DEBOUNCE_MS) {
        return;
    }

    bool down = rawState;   // signal is now stable

    // Once released, allow wasCancelHeld() to work (guards against instant cancel)
    if (!down) {
        buttonReleasedAfterStart = true;
    }

    // Falling edge: button just went down — record the time
    if (down && !buttonHeld) {
        buttonHeld       = true;
        buttonPressStart = millis();
        buttonShortPress = false;
    }

    // Rising edge: button just came up — was it a short tap?
    if (!down && buttonHeld) {
        buttonHeld = false;
        if ((millis() - buttonPressStart) < LONG_PRESS_MS) {
            buttonShortPress = true;   // consumed once by wasShortPress()
        }
    }
}

// Returns true once if a short press occurred, then resets the button state.
bool wasShortPress() {
    if (buttonShortPress) {
        buttonShortPress = false;
        return true;
    }
    return false;
}

// Returns true while the button has been held past LONG_PRESS_MS.
// The buttonReleasedAfterStart guard ensures the chef can't accidentally
// cancel the cycle the instant they let go of the start press.
bool wasCancelHeld() {
    return buttonHeld &&
           buttonReleasedAfterStart &&
           ((millis() - buttonPressStart) > LONG_PRESS_MS);
}


// ════════════════════════════════════════════════════════════════
//  TIMER INIT
// ════════════════════════════════════════════════════════════════

// Starts the two timers that run from the moment the device powers on.
// cycleTimer and cancelTimer are started later, triggered by button presses.
void initTimers() {
    loadingTimer.startTimer(LOADING_MS);
    tempReadTimer.startTimer(TEMP_READ_MS);
}


// ════════════════════════════════════════════════════════════════
//  STATE MACHINE
// ════════════════════════════════════════════════════════════════
//
// This is the brain of the device. Every loop, it checks the current
// screen and decides whether to stay or move to a new one.
//
// State map:
//
//   LOADING ──(7s)──> TEMP_MONITOR
//   TEMP_MONITOR ──(short press)──> CYCLE_IN_PROGRESS
//   CYCLE_IN_PROGRESS ──(timer done)──────────────────> SUCCESS
//   CYCLE_IN_PROGRESS ──(too cold)──> ALERT_LOW ──(ok)──> CYCLE_IN_PROGRESS
//   CYCLE_IN_PROGRESS ──(too hot)───> ALERT_HIGH ─(ok)──> CYCLE_IN_PROGRESS
//   ALERT_LOW  ──(timer done)──> TIMER_SUCCESS
//   ALERT_HIGH ──(timer done)──> TIMER_SUCCESS
//   Any active screen ──(hold 2s)──> CANCELLING ──(5s)──> TEMP_MONITOR
//   SUCCESS ──(short press)──> TEMP_MONITOR

void runStateMachine() {

    switch (currentScreen) {

        // Loading screen — wait for the timer to finish, then move to idle
        case SCREEN_LOADING:
            if (loadingTimer.isTimerReady()) {
                currentScreen = SCREEN_TEMP_MONITOR;
            }
            break;

        // Idle — watching temp, waiting for the chef to press start
        case SCREEN_TEMP_MONITOR:
            if (wasShortPress()) {
                // Reset button state so a lingering hold doesn't instantly cancel
                buttonHeld               = false;
                buttonPressStart         = 0;
                buttonReleasedAfterStart = false;
                cycleTimer.startTimer(CYCLE_MS);
                cycleStartTime = millis();
                currentScreen  = SCREEN_CYCLE_IN_PROGRESS;
            }
            break;

        // Cycle running — check for completion, temp problems, or cancel
        case SCREEN_CYCLE_IN_PROGRESS:
            if (cycleTimer.isTimerReady()) {
                cycleEndTime  = millis();
                currentScreen = SCREEN_SUCCESS;
                break;
            }
            if (tempF < TEMP_SAFE_LOW) {
                currentScreen = SCREEN_ALERT_LOW;
                break;
            }
            if (tempF > TEMP_SAFE_HIGH) {
                currentScreen = SCREEN_ALERT_HIGH;
                break;
            }
            if (wasCancelHeld()) {
                cancelTimer.startTimer(CANCEL_MS);
                currentScreen = SCREEN_CANCELLING;
            }
            break;

        // Too cold — timer still running, wait for temp to recover or cancel
        // Completion checked first so success is never missed
        case SCREEN_ALERT_LOW:
            if (cycleTimer.isTimerReady()) {
                cycleEndTime  = millis();
                currentScreen = SCREEN_TIMER_SUCCESS;
                break;
            }
            if (tempF >= TEMP_SAFE_LOW) {
                currentScreen = SCREEN_CYCLE_IN_PROGRESS;
                break;
            }
            if (wasCancelHeld()) {
                cancelTimer.startTimer(CANCEL_MS);
                currentScreen = SCREEN_CANCELLING;
            }
            break;

        // Too hot — same logic as too cold, just the opposite direction
        case SCREEN_ALERT_HIGH:
            if (cycleTimer.isTimerReady()) {
                cycleEndTime  = millis();
                currentScreen = SCREEN_TIMER_SUCCESS;
                break;
            }
            if (tempF <= TEMP_SAFE_HIGH) {
                currentScreen = SCREEN_CYCLE_IN_PROGRESS;
                break;
            }
            if (wasCancelHeld()) {
                cancelTimer.startTimer(CANCEL_MS);
                currentScreen = SCREEN_CANCELLING;
            }
            break;

        // Cancelling — show the animation, then return to idle
        case SCREEN_CANCELLING:
            if (cancelTimer.isTimerReady()) {
                currentScreen = SCREEN_TEMP_MONITOR;
            }
            break;

        // Success — wait for the chef to press the button to dismiss
        case SCREEN_SUCCESS:
            if (wasShortPress()) {
                currentScreen = SCREEN_TEMP_MONITOR;
            }
            break;

        // Timer success - 10 minutes has elapsed
        case SCREEN_TIMER_SUCCESS:
            if (wasShortPress()) {
                currentScreen = SCREEN_TEMP_MONITOR;
            }
            break;

        default:
            break;
    }
}


// ════════════════════════════════════════════════════════════════
//  ALERT FUNCTIONS (Hue bulbs + Wemo plugs)
// ════════════════════════════════════════════════════════════════

// Turns off alert lighting and hands stove control back to the chef.
void clearAlerts() {
    setHue(BULB1, false);
    setHue(BULB2, false);
    wemoWrite(WEMO_RADIO, LOW);
    wemoWrite(WEMO_STOVE, HIGH);   // stove back on — chef is in control
}

// Initiates Hue/Wemo commands only on screen transitions, not every frame.
// alertsActive prevents clearAlerts() from calling too many requests.
// 30+ times per second while nothing has actually changed.
void updateAlerts() {
    if (!WiFi.ready()) {
        return;
    }

    // If we've left an active screen, clear any lingering alerts
    bool inActive = (currentScreen == SCREEN_CYCLE_IN_PROGRESS ||
                     currentScreen == SCREEN_ALERT_LOW          ||
                     currentScreen == SCREEN_ALERT_HIGH);

    if (!inActive && alertsActive) {
        clearAlerts();
        alertsActive = false;
    }

    // TOO COLD transition: blue light + radio (audible cue in loud kitchen)
    if (currentScreen == SCREEN_ALERT_LOW && previousScreen != SCREEN_ALERT_LOW) {
        setHue(BULB1, true, HueBlue, 255, 255);
        wemoWrite(WEMO_RADIO, HIGH);
        alertsActive = true;
    }

    // TOO HOT transition: red light + radio + shut stove off
    if (currentScreen == SCREEN_ALERT_HIGH && previousScreen != SCREEN_ALERT_HIGH) {
        setHue(BULB1, true, HueRed, 255, 255);
        wemoWrite(WEMO_STOVE, LOW);
        wemoWrite(WEMO_RADIO, HIGH);
        alertsActive = true;
    }

    // Recovering back to in-range — cancel the alerts
    if (currentScreen == SCREEN_CYCLE_IN_PROGRESS &&
        (previousScreen == SCREEN_ALERT_LOW || previousScreen == SCREEN_ALERT_HIGH)) {
        clearAlerts();
        alertsActive = false;
    }

    previousScreen = currentScreen;
}


// ════════════════════════════════════════════════════════════════
//  DISPLAY FUNCTIONS
// ════════════════════════════════════════════════════════════════

// Starts up the OLED. Must be called in setup() before drawing anything.
void initDisplay() {
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.setTextColor(WHITE);
    display.clearDisplay();
    display.display();
}

// Draws the live temperature in the top-left corner.
// Big degF on the first row, smaller degC just below.
// The 16x16 icon in the top-right is drawn separately by each screen.
void drawTempBanner() {
    display.setTextSize(FONT_HERO);
    display.setCursor(TEMP_X, TEMP_Y);
    display.printf("%.0fF", tempF);

    display.setTextSize(FONT_BODY);
    display.setCursor(TEMPC_X, TEMPC_Y);
    display.printf("%.1fC", tempC);
}

// Draws a horizontal rule across the full display width.
// Separates the temperature area (top) from the message area (bottom).
void drawSeparator() {
    display.drawLine(0, SEPARATOR_Y, SCREEN_W - 1, SEPARATOR_Y, WHITE);
}

// Returns which animation frame we are on right now.
// Dividing millis() by speedMs slows the counter down.
// Wrapping with % numFrames keeps it cycling endlessly.
// Example: animFrame(200, 4) advances one frame every 200ms, cycling 0-1-2-3-0-1...
// "inline" tells the compiler to paste this code in-place rather than
// jumping to a separate function — tiny but worth it in a fast loop.
inline uint8_t animFrame(unsigned long speedMs, uint8_t numFrames) {
    return (uint8_t)((millis() / speedMs) % (unsigned long)numFrames);
}


// ════════════════════════════════════════════════════════════════
//  SCREEN FUNCTIONS
// ════════════════════════════════════════════════════════════════

// ── Screen 1: Loading screen ──────────────────────────────────────
// Shows while the device boots up. Displays the pot icon, animated
// steam puffs, the projet name, and the local boiling point once the
// BME280 has taken its first reading.
void displayLoadingScreen() {
    display.clearDisplay();

    // Pot icon — 40x40 pixels, top-left corner
    display.drawBitmap(0, 0, BITMAP_POT, 40, 40, WHITE);

    // ── Steam animation ───────────────────────────────────────────
    // Three 2x2 pixel "puffs" rise above the pot, staggered so they
    // look like separate bubbles rather than one block jumping upward.
    //
    // animFrame(150, 8) gives us a frame counter 0-7, advancing every 150ms.
    // Each puff uses its own offset into that counter (+0, +3, +6).
    // y = 30 - frame*3: starts near the pot lip, moves 3px up per frame,
    // then disappears once it rises above y=2 (the if-guard below).
    //
    // The three x positions (46, 52, 58) spread the puffs sideways.

    uint8_t f = animFrame(150, 8);   // master frame counter (0-7)

    uint8_t f1 = f;                  // puff 1: leads the group
    int     y1 = 30 - (int)f1 * 3;
    if (y1 >= 2) {
        display.fillRect(46, y1, 2, 2, WHITE);
    }

    uint8_t f2 = (f + 3) % 8;       // puff 2: 3 frames behind puff 1
    int     y2 = 30 - (int)f2 * 3;
    if (y2 >= 2) {
        display.fillRect(52, y2, 2, 2, WHITE);
    }

    uint8_t f3 = (f + 6) % 8;       // puff 3: 6 frames behind puff 1
    int     y3 = 30 - (int)f3 * 3;
    if (y3 >= 2) {
        display.fillRect(58, y3, 2, 2, WHITE);
    }

    // ── Text area (rows 42-63, below the pot) ─────────────────────
    display.setTextSize(FONT_BODY);

    display.setCursor(0, 42);
    display.printf("Saucier's Sentinel ");

    // Local boiling point — calculated from live air pressure.
    // boilingF is 0.0 until the BME280 reads; show a placeholder until ready.
    display.setCursor(0, 54);
    if (boilingF > 150.0) {
        display.printf("Boil pt: %.0fF", boilingF);
    }
    else {
        display.printf("Reading sensor...");
    }

    display.display();
}

// ── Screen 2: Idle temp monitor ───────────────────────────────────
// Shows live temperature and waits for the chef to press start.
void displayTempScreen() {
    display.clearDisplay();

    // Thermometer icon — top-right corner
    display.drawBitmap(ICON_X_RIGHT, ICON_Y, BITMAP_THERMO16, ICON_W, ICON_H, WHITE);

    drawTempBanner();
    drawSeparator();

    display.setTextSize(FONT_BODY);

    display.setCursor(0, ROW_STATUS);
    display.printf("Safe Range: %.0f-%.0fF", TEMP_SAFE_LOW, TEMP_SAFE_HIGH);

    display.setCursor(0, ROW_MSG1);
    display.printf(" "); // empty line for spacing or additional message

    display.setCursor(0, ROW_MSG2);
    display.printf("Press to start timer!");

    display.display();
}

// ── Screen 3: Cycle in progress ───────────────────────────────────
// Shows the countdown timer, a "hold to cancel" message,
// and a progress bar that fills from left to right as time passes.
void displayCycleScreen() {
    display.clearDisplay();

    // Hourglass icon — top-right corner
    display.drawBitmap(ICON_X_RIGHT, ICON_Y, BITMAP_HOURGLASS16, ICON_W, ICON_H, WHITE);

    drawTempBanner();
    drawSeparator();

    // Calculate remaining time
    // elapsed is clamped so it never accidentally exceeds CYCLE_MS
    unsigned long elapsed   = millis() - cycleStartTime;
    if (elapsed > CYCLE_MS) {
        elapsed = CYCLE_MS;
    }
    unsigned long remaining = CYCLE_MS - elapsed;
    unsigned long remMin    = remaining / 60000UL;
    unsigned long remSec    = (remaining % 60000UL) / 1000UL;

    display.setTextSize(FONT_BODY);

    display.setCursor(0, ROW_STATUS);
    display.printf("Left: %02lu:%02lu", remMin, remSec);

    display.setCursor(0, ROW_MSG1);
    display.printf("Hold 2s to cancel.");

    // Progress bar — only draw once there is at least 1px to fill.
    // This prevents an empty rectangle flashing on the very first frame.
    long filled = map((int)elapsed, 0, (int)CYCLE_MS, 0, (int)(PROG_W - 2));
    if (filled > 0) {
        display.drawRect(PROG_X, PROG_Y, PROG_W, PROG_H, WHITE);
        display.fillRect(PROG_X + 1, PROG_Y + 1, (int)filled, PROG_H - 2, WHITE);
    }

    display.display();
}

// ── Screen 4: Alert — too cold ────────────────────────────────────
// Timer keeps running. Thermometer icon shivers to signal cold.
// Timer stays visible — the chef can still use this as a kitchen timer.
void displayLowAlertScreen() {
    display.clearDisplay();

    // Thermometer shivers left/right by 1px each frame — looks cold!
    int shiver = (animFrame(SHIVER_MS, 2) == 0) ? -1 : 1;
    display.drawBitmap(ICON_X_RIGHT + shiver, ICON_Y,
                       BITMAP_THERMO16, ICON_W, ICON_H, WHITE);

    drawTempBanner();
    drawSeparator();

    // Timer — same calculation as the cycle screen
    unsigned long elapsed   = millis() - cycleStartTime;
    if (elapsed > CYCLE_MS) {
        elapsed = CYCLE_MS;
    }
    unsigned long remaining = CYCLE_MS - elapsed;
    unsigned long remMin    = remaining / 60000UL;
    unsigned long remSec    = (remaining % 60000UL) / 1000UL;

    display.setTextSize(FONT_BODY);

    // Timer on the left, alert label fills the row — 21 chars exactly
    display.setCursor(0, ROW_STATUS);
    display.printf("%02lu:%02lu  Temp Low!", remMin, remSec);

    display.setCursor(0, ROW_MSG1);
    display.printf(" "); // empty line for spacing or additional message
    display.setCursor(0, ROW_MSG2);
    display.printf("Hold 2s to cancel.");

    display.display();
}

// ── Screen 5: Alert — too hot ─────────────────────────────────────
// Timer keeps running. Small flame flickers to signal excess heat.
void displayHighAlertScreen() {
    display.clearDisplay();

    display.drawBitmap(ICON_X_RIGHT, ICON_Y, BITMAP_THERMO16, ICON_W, ICON_H, WHITE);

    drawTempBanner();
    drawSeparator();

    // Timer — same as low alert
    unsigned long elapsed   = millis() - cycleStartTime;
    if (elapsed > CYCLE_MS) {
        elapsed = CYCLE_MS;
    }
    unsigned long remaining = CYCLE_MS - elapsed;
    unsigned long remMin    = remaining / 60000UL;
    unsigned long remSec    = (remaining % 60000UL) / 1000UL;

    display.setTextSize(FONT_BODY);

    display.setCursor(0, ROW_STATUS);
    display.printf("%02lu:%02lu  Temp High!", remMin, remSec);

    display.setCursor(0, ROW_MSG1);
    display.printf(" "); // empty line for spacing or additional message

    display.setCursor(0, ROW_MSG2);
    display.printf("Hold 2s to cancel.");

    // Flame flicker — two small pixel clusters alternate position each frame
    if (animFrame(FLASH_MS, 2) == 0) {
        display.fillRect(ICON_X_RIGHT - 6, ROW_STATUS,     4, 4, WHITE);
        display.fillRect(ICON_X_RIGHT - 2, ROW_STATUS + 4, 3, 3, WHITE);
    }
    else {
        display.fillRect(ICON_X_RIGHT - 8, ROW_STATUS + 2, 3, 3, WHITE);
        display.fillRect(ICON_X_RIGHT - 4, ROW_STATUS,     4, 4, WHITE);
    }

    display.display();
}

// ── Screen 6: Cancelling ──────────────────────────────────────────
// Full-screen message — no temp banner needed since the cycle is ending.
// A spinning /-\| character animates to show something is happening.
void displayCancellingScreen() {
    display.clearDisplay();

    // Cancel icon — top-right corner
    display.drawBitmap(ICON_X_RIGHT, ICON_Y, BITMAP_CANCEL16, ICON_W, ICON_H, WHITE);

    // "CANCELLING" in large text, centered vertically on this unique layout.
    // y=24 is below the 16px icon and above the halfway point of the screen.
    display.setTextSize(FONT_HEADLINE);
    display.setCursor(0, 24);
    display.printf("CANCELLING");

    // Spinner — cycles through /-\| characters in the top-right area
    const char spinChars[4] = { '/', '-', '\\', '|' };
    display.setCursor(100, 2);
    display.printf("%c", spinChars[animFrame(SPINNER_MS, 4)]);

    display.display();
}

// ── Screen 7: Success! ────────────────────────────────────────────
// The chef made it! 10 minutes at temperature — safe to bottle.
// Shows elapsed time, the shield icon, and corner sparkles.
void displaySuccessScreen() {
    display.clearDisplay();

    // Shield bitmap (40x40) — right-aligned with a 1px margin.
    // x = 128(screen) - 40(bitmap) - 1(margin) = 87
    display.drawBitmap(87, 2, BITMAP_SHIELD, 40, 40, WHITE);

    // Sparkles — two small dots blink in and out beside the shield
    if (animFrame(ANIM_SPEED, 2) == 0) {
        display.fillRect(108, 11,            2, 2, WHITE);
        display.fillRect(ICON_X_RIGHT - 4, 30, 2, 2, WHITE);
    }

    display.setTextSize(FONT_HEADLINE);
    display.setCursor(0, TEMP_Y);
    display.printf("BRAVO!");

    // How long did the cycle actually take?
    unsigned long total  = cycleEndTime - cycleStartTime;
    unsigned long totMin = total / 60000UL;
    unsigned long totSec = (total % 60000UL) / 1000UL;

    display.setTextSize(FONT_BODY);

    display.setCursor(0, ROW_STATUS);
    display.printf("Time: %02lu:%02lu", totMin, totSec);

    display.setCursor(0, ROW_MSG1);
    display.printf("Safe to bottle!");

    display.setCursor(0, ROW_MSG2);
    display.printf("Press to go.");

    display.display();
}

// ── Screen 7: Timer Success ────────────────────────────────────────────
// The chef can also use the device as a general kitchen timer, when temp is out of range.

void displayTimerSuccessScreen() {
    display.clearDisplay();

    display.drawBitmap(87, 24, BITMAP_POT, 40, 40, WHITE);

    display.setTextSize(FONT_HEADLINE);
    display.setCursor(0, TEMP_Y);
    display.print("COMPLETE!");


    unsigned long total  = cycleEndTime - cycleStartTime;
    unsigned long totMin = total / 60000UL;
    unsigned long totSec = (total % 60000UL) / 1000UL;

    display.setTextSize(FONT_BODY);
    display.setCursor(0, ROW_STATUS);
    display.printf("Time: %02lu:%02lu", totMin, totSec);

    display.setCursor(0, ROW_MSG1);
    display.print(" "); // empty line for spacing or additional message

    display.setCursor(0, ROW_MSG2);
    display.print("Press to go.");

    display.display();
}


// ════════════════════════════════════════════════════════════════
//  LED FUNCTION
// ════════════════════════════════════════════════════════════════

// Controls the D7 power LED using millis() — no delay(), nothing blocks.
// The LED speaks a simple language the chef can read at a glance:
//
//   Solid ON    — device is idle or loading, waiting for you
//   Slow tick   — timer is running (one brief blink per second)
//   Fast blink  — success! cycle complete, press button to dismiss
//   Off         — cancelling in progress
void updateLED() {
    unsigned long now = millis();

    if (currentScreen == SCREEN_CYCLE_IN_PROGRESS ||
        currentScreen == SCREEN_ALERT_LOW          ||
        currentScreen == SCREEN_ALERT_HIGH) {

        // Slow heartbeat — ON for 900ms, off for 100ms, once per second.
        // now % 1000 cycles 0 to 999 over and over, one full cycle per second.
        // The brief off-flash is subtle but just visible enough to count seconds.
        if ((now % 1000) < 900) {
            digitalWrite(POWER_LEDPIN, HIGH);
        }
        else {
            digitalWrite(POWER_LEDPIN, LOW);
        }
    }
    else if (currentScreen == SCREEN_SUCCESS) {

        // Fast celebratory blink — on 100ms, off 100ms.
        // now % 200 cycles 0 to 199; on for the first half, off for the second.
        if ((now % 200) < 100) {
            digitalWrite(POWER_LEDPIN, HIGH);
        }
        else {
            digitalWrite(POWER_LEDPIN, LOW);
        }
    }
    else if (currentScreen == SCREEN_TIMER_SUCCESS) {

        // Fast celebratory blink — 100ms on, 100ms off.
        // now % 200 cycles 0→199. ON for first half, OFF for second.
        if ((now % 200) < 100) {
            digitalWrite(POWER_LEDPIN, HIGH);
        } else {
            digitalWrite(POWER_LEDPIN, LOW);
        }
    }
    else if (currentScreen == SCREEN_CANCELLING) {

        // LED off — something ended, not nothing started
        digitalWrite(POWER_LEDPIN, LOW);
    }
    else {

        // All other screens (loading, idle): solid on — just a power indicator
        digitalWrite(POWER_LEDPIN, HIGH);
    }
}
