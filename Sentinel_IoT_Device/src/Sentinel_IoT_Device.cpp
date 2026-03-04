/* 
 * Project: Saucemakers_Sentinel__a_temp_monitor
 * Author: Nicole
 * Date: 03/02/2026
 * Description: Keep your bottling temp in range! 
 * Temp is sensed, monitored, and displayed, 
 * while the timer counts down. 
 * You're alerted if your temp goes awry!
 */

// TODO:

// to get it functioning right: 

// what to put in timer loop, what not to

// detail out switch 

// Add in Hues

// finish Wemo

// other: 

// optimize screen displays - placements of things

// formatting

// delete unnecessary things




// Include Particle Device OS APIs
#include "Particle.h"
//#include "MAX6675_RK.h"
#include "MAX6675.h"
#include "Adafruit_BME280.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "neopixel.h"
#include "IoTTimer.h"
#include "Button.h"
#include "bitmaps.h"
#include "wemo.h"   

//#include <math.h> 

SYSTEM_MODE(SEMI_AUTOMATIC);
//SYSTEM_MODE(MANUAL); //control logging into classroom router);

SYSTEM_THREAD(ENABLED);

//Defining the device-is-powered-on indicator LED pin
const int POWER_LEDPIN = D2;

//Defining the thermocouple object, variables, and CS pin
const int THERMO_CSPIN = D5; //CS pin for the thermocouple
MAX6675 thermocouple;
byte status;
float tempC, tempF;
float tempHigh = 200.00; //high temp threshold for alerts, in F
float tempLow = 185.00; //low temp threshold for alerts, in F
float boilingPointF = 212.0; // variable to store calculated boiling point based on altitude

//Defining the BME environmental sensor object
Adafruit_BME280 bme;
const int hexAddress = 0x76; //address for the BME
bool statusBME; //variable to make sure BME is working
float pressPA, pressInHg, humidRH; //variables for BME280 readings, stored for current and future features

//Defining the IoTTimer object
IoTTimer timer; 
unsigned int msec = 500; //variable for the IoTTimer
int unsigned long timerStartTime = 0; //variable to track when the cycle timer starts   

//Defining OLED display object
const int OLED_RESET=-1;
Adafruit_SSD1306 display(OLED_RESET);

// //Defining the NeoPixel object and constants (decided to use LED bc of SPI situ)
// const int PIXELCOUNT = 1;
// Adafruit_NeoPixel pixel(PIXELCOUNT, SPI1, WS2812B);
// const int blue = 0x0000FF; //cool
// const int green = 0x00FF00; //comfortable
// const int yellow = 0xFFFF00; //warm
// const int red = 0xFF0000; //hot

//Defining the Wemo and Hue objects, constants, and variables
const int STOVEWEMO=3; //Which wemo to control
const int RADIOWEMO=4; //Which wemo to control
bool onOFF = false; // internal state variable to track whether wemo is on or off

//Defining the button object and long press variables for start/cancel button
const int BUTTON_PIN = D3; // Pin for the Start/Cancel button
Button startOrCancelCycleButton(BUTTON_PIN); //one press starts timer, long press cancels
unsigned long buttonPressStartTime = 0; //for timing the long press
bool isButtonBeingHeld = false; 
const unsigned long LONG_PRESS_CANCEL = 2000; // Press to cancel (hold for 2 seconds)

//Defining events that trigger Wemos and Hues
const char* EVENT_STOVE_OFF = "turnStoveOff";
const char* EVENT_RADIO_ON = "turnRadioOn";
const char* EVENT_RADIO_OFF = "turnRadioOff";
const char* EVENT_HUE_ALERT_ON = "hueAlertOn";
const char* EVENT_HUE_ALERT_OFF = "hueAlertOff";

// constants to reference the current state of the OLED display
enum ScreenState {
    SCREEN_LOADING,
    SCREEN_TEMP_MONITOR,
    SCREEN_CYCLE_IN_PROGRESS,
    SCREEN_ALERT_LOW,
    SCREEN_ALERT_HIGH,
    SCREEN_CANCELLING,
    SCREEN_SUCCESS,
};

//declare custom functions
void displayLoadingScreen();
void displayTempScreen();
void displayCycleScreen();
void displayLowAlertScreen();
void displayHighAlertScreen(); 
void displayCancellingScreen();
void displaySuccessScreen();
void longPressCancelCycle();

ScreenState currentScreen = SCREEN_LOADING; // When device powers on, this screen shows

void setup() {

    //Initiate serial monitor
    Serial.begin(9600);
    waitFor(Serial.isConnected,10000);

    // Turn on the device-is-powered-on indicator LED 
    pinMode(POWER_LEDPIN, OUTPUT);
    digitalWrite(POWER_LEDPIN, HIGH); 

    //Initiate Thermocouple
    thermocouple.begin(THERMO_CSPIN); // Use D5 as Chip Select (CS) for SPI

    //initiate the BME280 (environmental sensor)
    statusBME = bme.begin(hexAddress);
    // if (statusBME  == false){    //uncomment for testing BME280
    //     Serial.printf("BME at address0x%02X failed to start", hexAddress);
    // }

    // initiate the IoTTimer
    timer.startTimer(msec);

    // initialize the OLED display
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.setTextColor(WHITE);
    display.clearDisplay();
    display.setTextSize(1);
    display.display();

    // Initialize the wemo - have it connect to wifi, clear any previous credentials, and set it to our router, IoTNetwork
    pinMode(D7, OUTPUT); // set D7 as the button to turn on and off the wemo, outputs a click.
    WiFi.on();
    //WiFi.clearCredentials();           // uncomment to clear previous wifi creds when needed 
    WiFi.setCredentials("IoTNetwork"); // set the credentials to our network
    WiFi.connect();
   

    //Initialize HUE bulbs

}

void loop() {

    if (timer.isTimerReady()) {
    timer.startTimer(500);   // restart timer after 500ms

    if (currentScreen == SCREEN_CYCLE_IN_PROGRESS ||
        currentScreen == SCREEN_SUCCESS) {

        unsigned long timeSoFar = millis() - timerStartTime;
    //is this a good way to do this? 
        if (timeSoFar >= 600000 && currentScreen == SCREEN_CYCLE_IN_PROGRESS) {
            currentScreen = SCREEN_SUCCESS;
        }
    }
}
    //delete soon not needed anymore as I'm moving commands into SWITCH
    // // Start Cycle button with long press cancel 
    // if (startOrCancelCycleButton.isClicked()) {
    //     // Quick click 
    //     if (currentScreen == SCREEN_TEMP_MONITOR) {
    //         currentScreen = SCREEN_CYCLE_IN_PROGRESS;
    //         timerStartTime = millis();
    //         timer.startTimer(500);
    //     } else if (currentScreen == SCREEN_CANCELLING) {
    //         // what if a person wants to cancel the cancelling?
    //     }
    // }

    // OLED Display during each event
    switch (currentScreen) {
        case SCREEN_LOADING:
            displayLoadingScreen(); //a function that displays the loading screen
            // Wait a few seconds and go to the next screen
            if (millis() > 20000) { // Show loading for 5 seconds
                 currentScreen = SCREEN_TEMP_MONITOR;
            }
            break;
        case SCREEN_TEMP_MONITOR:
            displayTempScreen(); //fn that shows just the temp
            if (tempF < tempLow) {
                currentScreen = SCREEN_ALERT_LOW;
            } else if (tempF > tempHigh) {
                currentScreen = SCREEN_ALERT_HIGH;
            } else if (startOrCancelCycleButton.isClicked()) {
                currentScreen = SCREEN_CYCLE_IN_PROGRESS;
                timerStartTime = millis();
                timer.startTimer(500);
            } else if (isButtonBeingHeld) {
                longPressCancelCycle(); //check for long press to cancel
            }   
            break;
        case SCREEN_CYCLE_IN_PROGRESS:
            displayCycleScreen(); //fn that shows temp and timer counting up
            // temp causes screen to go to either alert low, alert high, cancel, or success
            break;
        case SCREEN_ALERT_LOW: 
            displayLowAlertScreen(); //fn that shows low temp alert screen
            break;
        case SCREEN_ALERT_HIGH: 
            displayHighAlertScreen(); //fn that shows high temp alert screen
            break;
        case SCREEN_CANCELLING: 
            displayCancellingScreen(); //fn that shows cancel screen, makes you wait

            break;
        case SCREEN_SUCCESS: 
            displaySuccessScreen(); //fn that shows a sucess screen, timer still counting up
            break;
        default:
            displayTempScreen(); //default to temp screen just in case
    }

    //uncomment this block for testing
        // //Read temp from thermocouple
        // status = thermocouple.read();
        // if (status != STATUS_OK) {
        //     Serial.printf("ERROR!\n");
        // }
        //tempC = thermocouple.getTemperature();
        //tempF = (9.0/5.0)* tempC + 32;
        //Serial.printf("Temperature:%0.2f or %0.2f\n",tempC,tempF);
    }

    //custom functions here

    void displayLoadingScreen() {
        // Opening Screen: Left image is a steaming pot. Title is: Saucemaker's Sentinel. 
        // Right is a sentinel shield graphic with spoon and knife instead of swords. 
        // Lower text: boiling point here is [insert boiling point according to altitude]
        display.clearDisplay();
        display.drawBitmap(0, 0, BITMAP_POT, 40, 40, WHITE); // Left image
        display.drawBitmap(88, 0, BITMAP_SHIELD, 40, 40, WHITE); // Right image
        display.setCursor(0, 45);
        display.setTextSize(1);
        display.printf("Saucemaker's Sentinel\n");
        //display.printf("Boiling point: %.1f F", boilingPointF); doesn't fit
        display.display();      
    }     
    
    void displayTempScreen  () {
        // Temperature screen: Left image is a thermometer. 
        // Temp is displayed in F and C, as large as possible. 
        // Lower text says: Press Start to begin monitoring
        display.clearDisplay();

        // Read TC here for most up-to-date value before display
        thermocouple.read();
        tempC = thermocouple.getTemperature();
        tempF = (9.0/5.0)* tempC + 32;

        display.drawBitmap(0, 0, BITMAP_THERMO, 40, 40, WHITE); // Thermometer graphic
        display.setTextSize(3);
        display.setCursor(45, 10);
        display.printf("%.0fF", tempF);
        display.setTextSize(1);
        display.setCursor(45, 35);
        display.printf("%.0fC", tempC);

        display.setCursor(0, 55);
        display.printf("Press Start to begin monitoring");
        display.display();
    }

    void displayCycleScreen() {
        // CYCLE IN PROGRESS: On the left is the thermometer graphic still. 
        //Also the temp is still displayed. And also a timer is there counting up. 
        display.clearDisplay();
        // Read TC here
        thermocouple.read();
        tempC = thermocouple.getTemperature();
        tempF = (9.0/5.0)* tempC + 32;

        display.drawBitmap(0, 0, BITMAP_THERMO, 40, 40, WHITE); // Thermometer graphic
        display.setTextSize(2);
        display.setCursor(45, 5);
        display.printf("%.0fF", tempF);
        
        // Timer display
        unsigned long timeSoFar = millis() - timerStartTime;
        unsigned long minutes = timeSoFar / 60000;
        unsigned long seconds = (timeSoFar % 60000) / 1000;
        display.setCursor(0, 30);
        display.printf("Time: %02lu:%02lu", minutes, seconds); //lu is unsigned long
        display.display();
    }

    void displaySuccessScreen() {
        // SUCCESS SCREEN: Left image is a shield with a check mark on it. 
        //Don't think I have room for a graphic, unless it was momentary
        // SUCCESS! Your sauce is in range. 
        // Timer is still displayed at the bottom, counting up.
        display.clearDisplay();

        unsigned long timeSoFar = millis() - timerStartTime;
        unsigned long minutes = timeSoFar / 60000;
        unsigned long seconds = (timeSoFar % 60000) / 1000;

        display.drawBitmap(0, 0, BITMAP_SHIELD, 40, 40, WHITE);

        display.setTextSize(2);
        display.setCursor(45, 5);
        display.print("SUCCESS");

        display.setTextSize(1);
        display.setCursor(0, 40);
        display.printf("Time: %02lu:%02lu", minutes, seconds);

        display.display();
    }

    void displayLowAlertScreen() {
        // LOW TEMP ALERT: Left image is a thermometer [could add a snowflake on it one day] 
        // Text says: Too cold! Turn up the heat! //is there room for text?
        display.clearDisplay();
        // Read TC here
        thermocouple.read();
        tempC = thermocouple.getTemperature();
        tempF = (9.0/5.0)* tempC + 32;
        display.drawBitmap(0, 0, BITMAP_THERMO, 40, 40, WHITE); // Thermometer graphic
        display.setTextSize(2);
        display.setCursor(45, 5);
        display.printf("%.0fF", tempF);
        display.setTextSize(1);
        display.setCursor(0, 35);
        display.printf("Too cold! Turn up the heat!");
        display.display();
    }
    
    void displayHighAlertScreen() {
        // HIGH TEMP ALERT: Left image is a thermometer [could add a flame on it]
        // Text says: Too hot! Turn down the heat! // is room for text?
        display.clearDisplay();
        // Read TC here
        thermocouple.read();
        tempC = thermocouple.getTemperature();
        tempF = (9.0/5.0)* tempC + 32;
        display.drawBitmap(0, 0, BITMAP_THERMO, 40, 40, WHITE); // Thermometer graphic
        display.setTextSize(2);
        display.setCursor(45, 5);
        display.printf("%.0fF", tempF);
        display.setTextSize(1);
        display.setCursor(0, 35);
        display.printf("Too hot! Turn down the heat!");
        display.display();
    }

    void displayCancellingScreen() {
        // CANCELLING SCREEN: Left image is a waiting timer graphic with hourglass
        // Text says: Cancelling... Please wait.
        display.clearDisplay();
        display.drawBitmap(0, 0, BITMAP_CANCEL, 40, 40, WHITE); // Cancel graphic
        display.setTextSize(1);
        display.setCursor(0, 45);
        display.printf("Cancelling... Please wait.");
        display.display();
    }

    void longPressCancelCycle() {
        // If the button is being held and the time exceeds the long press threshold, cancel the cycle
        if (isButtonBeingHeld and (millis() - buttonPressStartTime >= LONG_PRESS_CANCEL)) {
            currentScreen = SCREEN_CANCELLING;
            isButtonBeingHeld = false; // reset the hold state
        }
    }
    


 

