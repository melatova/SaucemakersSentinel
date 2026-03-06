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

// detail out switch 

// other: 

// optimize screen displays - placements of things

// formatting

// delete unnecessary things

// timers - some issues. .  match correct timers to purpose

//  move updating temp, checking for long press, updating internal state, and drawing screen to loop





/////////////--------------- INCLUDE LIBRARIES ---------------////////////////



#include "Particle.h" // Include Particle Device OS APIs
#include "MAX6675.h" // Thermocouple library
#include "Adafruit_BME280.h" // BME280 environmental sensor library
#include "Adafruit_GFX.h" // Adafruit graphics library for OLED display
#include "Adafruit_SSD1306.h" // Adafruit library for SSD1306 OLED display
#include "IoTTimer.h" // IoTTimer library for timing events
#include "Button.h" // Button library for handling button input
#include "bitmaps.h" // Bitmap graphics for OLED display
#include "wemo.h"  // Wemo control library
#include "hue.h" // Hue control library




/////////////--------------- DEFINE SYSTEM MODE AND THREADING ---------------////////////////



//  Set system mode, when to connect to wifi, whether to run code before connected
SYSTEM_MODE(SEMI_AUTOMATIC);

//  SYSTEM_MODE(MANUAL); //control logging into classroom router, add delay if needed);

//  Whether to run code in separate threads before connected.
//      Enabled allow code to execute before fully connected
SYSTEM_THREAD(ENABLED); //include delay in setup if wifi needs more connection time for Hues and Wemos




///////////--------------- THERMOCOUPLE AND BME OBJECTS ---------------///////////



//  Defining the device-is-powered-on indicator LED pin
const int POWER_LEDPIN = D2;

//  Defining the thermocouple object, variables, and CS pin
const int THERMO_CSPIN = D5; //CS pin for the thermocouple
MAX6675 thermocouple;
byte status; 
float tempC, tempF;
float tempHigh = 200.00; //high temp threshold for alerts, in F
float tempLow = 185.00; //low temp threshold for alerts, in F
float boilingPointF = 212.0; // variable to store calculated boiling point based on altitude

//  Defining the BME environmental sensor object
Adafruit_BME280 bme;
const int hexAddress = 0x76; //address for the BME
bool statusBME; //variable to make sure BME is working
float pressPA, pressInHg, humidRH; //variables for BME280 readings, stored for current and future features





///////////--------------- TIMER OBJECTS ---------------///////////



//  Defining the IoTTimer object for holding the loading screen
//      Hold loading screen for 20 seconds, then go to temp screen
IoTTimer loadingScreenTimer;
unsigned int loadingScreenMsec = 20000; //variable for how long the loading screen is held
int unsigned long loadingScreenStartTime = 0; //variable to track when the loading screen starts     

//  Defining the IoTTimer object for reading the thermocouple every half second
IoTTimer tempReadTimer; 
unsigned int tempReadMsec = 500; //variable for the IoTTimer
int unsigned long tempReadStartTime = 0; //variable to track when the cycle timer starts  

//  Defining the IoTTimer object for the monitoring cycle, which is 10 minutes, or 600,000 milliseconds
IoTTimer hotProcessMonitorTimer;
unsigned int cycleMsec = 600000; //variable for how long the monitoring cycle is
int unsigned long cycleStartTime = 0; //variable to track when the monitoring cycle starts

//  Defining IoTTimer object for long press to cancel cycle
IoTTimer longPressCancelTimer;
unsigned int longPressMsec = 2000; //variable for how long the long press to cancel needs to be
int unsigned long longPressStartTime = 0; //variable to track when the long press to cancel starts

//  Defining the IoTTImer object for holding the Cancel screen
//      Hold cancel screen for 5 seconds, then go back to temp screen
//      This gives time for processes to cancel and time for the user to see the cancel screen before it goes away.
IoTTimer cancelWaitTimer;
unsigned int cancelWaitMsec = 5000; //variable for how long the cancel screen is held
int unsigned long cancelWaitStartTime = 0; //variable to track when the cancel screen starts





///////////--------------- OLED, HUE, WEMO, AND BUTTON OBJECTS ---------------///////////



//  Defining OLED display object
const int OLED_RESET=-1;
Adafruit_SSD1306 display(OLED_RESET);

//  Defining the Hue objects
const int BULB1 = 1;
const int BULB2 = 2;

//  Defining the Wemo and Hue objects, constants, and variables
const int WEMO_STOVE=3; //Which wemo to control
const int WEMO_RADIO=4; //Which wemo to control
bool onOFF = false; // internal state variable to track whether wemo is on or off

//  Defining the button object and long press variables for start/cancel button
const int BUTTON_PIN = D3; // Pin for the Start/Cancel button
Button startOrCancelCycleButton(BUTTON_PIN); //one press starts timer, long press cancels
unsigned long buttonPressStartTime = 0; //for timing the long press
bool isButtonBeingHeld = false; 
const unsigned long LONG_PRESS_CANCEL = 2000; // Press to cancel (hold for 2 seconds)



///////////--------------- SCREEN STATE MANAGEMENT ---------------///////////



//  Internal state constants to keep track of the current state of the OLED display
enum ScreenState {
    SCREEN_LOADING,
    SCREEN_TEMP_MONITOR,
    SCREEN_CYCLE_IN_PROGRESS,
    SCREEN_ALERT_LOW,
    SCREEN_ALERT_HIGH,
    SCREEN_CANCELLING,
    SCREEN_SUCCESS,
};

//  Declare custom functions for OLED display and long press cancel
void updateTemperature();
void displayLoadingScreen();
void displayTempScreen();
void displayCycleScreen();
void displayLowAlertScreen();
void displayHighAlertScreen(); 
void displayCancellingScreen();
void displaySuccessScreen();
void longPressCancelCycle();

ScreenState currentScreen = SCREEN_LOADING; // set inital screen state when powering on


void setup() {

//////////////////------------INITIALIATIONS THAT RUN ONCE---------------////////////////

    //  Initiate serial monitor
    Serial.begin(9600);
    waitFor(Serial.isConnected,10000);

    //  Turn on the powered on indicator LED 
    pinMode(POWER_LEDPIN, OUTPUT);
    digitalWrite(POWER_LEDPIN, HIGH); 

    //  Initiate Thermocouple
    thermocouple.begin(THERMO_CSPIN); // Use D5 as Chip Select (CS) for SPI

    //  Initiate the BME280 (environmental sensor)
    statusBME = bme.begin(hexAddress);
    // if (statusBME  == false){    //uncomment for testing BME280
    //     Serial.printf("BME at address0x%02X failed to start", hexAddress);
    // }

    //  start the IoTTimers
   millis(); //start the millis timer for the IoTTimers to use for timing events

    //  initialize the OLED display
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.setTextColor(WHITE);
    display.clearDisplay();
    display.setTextSize(1);
    display.display(); 

    //  Initialize the wemo and the Hue bulbs - have them connect to wifi, 
    //      clear any previous credentials if needed, and set it to our router, IoTNetwork
    pinMode(D7, OUTPUT); // set D7 as the button to turn on and off the wemo, outputs a click.
    WiFi.on();
    //WiFi.clearCredentials();           // uncomment to clear previous wifi creds when needed 
    WiFi.setCredentials("IoTNetwork"); // set the credentials to our network
    WiFi.connect();
    // while(WiFi.connecting()) {    // uncomment for testing wifi connection
    //     Serial.printf(".");
    // }
    // Serial.printf("\n\n");
}

void loop() {
//////////////------------MAIN LOOP THAT RUNS CONTINUOUSLY---------------////////////////
    
    
    //  OLED Display during each event and calls functions to update the display 
    //      and take readings at the appropriate times
    //  Start all timers here, not in custom functions
    switch (currentScreen) {    
        case SCREEN_LOADING:
           {}
            if (!loadingScreenTimer.isTimerReady()){
                loadingScreenTimer.startTimer(loadingScreenMsec); //start the timer for how long the loading screen is held
            }
            displayLoadingScreen(); //a function that displays the loading screen
            if (loadingScreenTimer.isTimerReady()) { //if 20 seconds has passed, go to temp screen      
                 currentScreen = SCREEN_TEMP_MONITOR;
            }   

            break;

        case SCREEN_TEMP_MONITOR:
            displayTempScreen(); //fn that shows just the temp
            if (startOrCancelCycleButton.isClicked() && !isButtonBeingHeld) {
                currentScreen = SCREEN_CYCLE_IN_PROGRESS;
                
            break;
            }

        case SCREEN_CYCLE_IN_PROGRESS:
            displayCycleScreen(); //fn that shows temp and timer counting up

    
            if  (hotProcessMonitorTimer.isTimerReady()) { //if 10 min passes, cycle is complete 
                
                currentScreen = SCREEN_SUCCESS;
            }
        
            if (startOrCancelCycleButton.isClicked()) { //use is clicked or is pressed? 
                    longPressStartTime = millis();
                    isButtonBeingHeld = true;
                //go to cancelling screen, which will also cancel the cycle and reset things
                currentScreen = SCREEN_CANCELLING; 
            }
            break;

        case SCREEN_ALERT_LOW: 
            displayLowAlertScreen(); //fn that shows low temp alert screen
            break;
            
        case SCREEN_ALERT_HIGH: 
            displayHighAlertScreen(); //fn that shows high temp alert screen
            break;
            
        case SCREEN_CANCELLING: 
            displayCancellingScreen(); //fn that shows cancel screen
            if (cancelWaitTimer.isTimerReady()) { //if 5 seconds has passed, go back to temp monitor screen
            currentScreen = SCREEN_TEMP_MONITOR; //go back to temp monitor after cancelling
            }
             
            break;
        case SCREEN_SUCCESS: 
            displaySuccessScreen(); //fn that shows a sucess screen, timer still counting up
            break;

        default:
            displayTempScreen(); //default to temp screen just in case
            break;
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





   /////////////-----------CUSTOM FUNCTIONS FOR DIFFERENT CASES----------//////////////

   //----------FUNCTION TO UPDATE TEMPERATURE READINGS and convert C to F-------//
    void updateTemperature() {
        if (tempReadTimer.isTimerReady()) {
            thermocouple.read();
            tempC = thermocouple.getTemperature();
            tempF = (9.0/5.0) * tempC + 32;
            tempReadTimer.startTimer(tempReadMsec);
        }
    }

    //----------FUNCTION TO CHECK FOR LONG PRESS TO CANCEL CYCLE-------//
    //Call this in the main loop
    void checkButtonHold() {

    if (startOrCancelCycleButton.isPressed()) {

        if (!isButtonBeingHeld) {
            buttonPressStartTime = millis();
            isButtonBeingHeld = true;
        }

        if (millis() - buttonPressStartTime > LONG_PRESS_CANCEL) {
            currentScreen = SCREEN_CANCELLING;
        }

    } else {
        isButtonBeingHeld = false;
    }

}

    void displayLoadingScreen() {

    //----LOADING SCREEN Shows logo and project name-------//

        //start the timer for how long the loading screen is held
        loadingScreenTimer.startTimer(loadingScreenMsec);   
        
         // Read TC here for most up-to-date value before display

        display.clearDisplay();
        display.drawBitmap(0, 0, BITMAP_POT, 40, 40, WHITE); // Left image
        display.drawBitmap(88, 0, BITMAP_SHIELD, 40, 40, WHITE); // Right image
        display.setCursor(0, 45);
        display.setTextSize(1);
        display.printf("Saucemaker's Sentinel\n");
        //display.printf("Boiling point: %.1f F", boilingPointF); doesn't fit
        display.display();  
        // Redundancy to kill any lingering alerts, reset wemos, and turn off hue bulbs
        setHue(BULB1,false); //Set Hue bulb1 to off
        setHue(BULB2,false); //Set Hue bulb2 to off
        wemoWrite(WEMO_RADIO, LOW);    
    }     
    



    void displayTempScreen  () {

    //----TEMP SCREEN Shows just the temp-------//


        display.clearDisplay();

        // Read TC here for most up-to-date value before display
            // Start timer for TC readings for every half-second
        updateTemperature();       

        // Draw screen with image and temp readings
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

        // Redundancy to kill any lingering alerts, reset wemos, and turn off hue bulbs
        setHue(BULB1,false); //Set Hue bulb1 to off
        setHue(BULB2,false); //Set Hue bulb2 to off
        wemoWrite(WEMO_RADIO, LOW);
    }



    void displayCycleScreen() {
        
    //----CYCLE IN PROGRESS SCREEN Shows temp, and timer counting up-------//

        display.clearDisplay();

        //start the cycle timer for 10 minutes
        hotProcessMonitorTimer.startTimer(cycleMsec);      
        cycleStartTime = millis(); //track when the cycle starts for the timer display
        isButtonBeingHeld = false; //confirm the button hold state was not a long press 
            
        // Read TC here for most up-to-date value before display
        // Start timer for TC readings for every half-second
        updateTemperature();

        display.drawBitmap(0, 0, BITMAP_THERMO, 40, 40, WHITE); // Thermometer graphic
        display.setTextSize(2);
        display.setCursor(45, 5);
        display.printf("%.0fF", tempF);
        
        // Timer display
        unsigned long timeSoFar = millis() - cycleStartTime;
        unsigned long minutes = timeSoFar / 60000;
        unsigned long seconds = (timeSoFar % 60000) / 1000;
        display.setCursor(0, 30);
        display.printf("Time: %02lu:%02lu", minutes, seconds); //lu is unsigned long
        display.display();

        // Redundancy to kill any lingering alerts, reset wemos, and turn off hue bulbs
        setHue(BULB1,false); //Set Hue bulb1 to off
        setHue(BULB2,false); //Set Hue bulb2 to off
        wemoWrite(WEMO_RADIO, LOW);
    }



    void displaySuccessScreen() {
    
    //----CYCLE SUCCESS SCREEN Shows temp, and timer counting up-------//


        display.clearDisplay();

        // Read TC here for most up-to-date value before display
        // Start timer for TC readings for every half-second
        updateTemperature();

        unsigned long cycleStartTime = millis() - cycleStartTime; 
        unsigned long minutes = cycleStartTime / 60000;
        unsigned long seconds = (cycleStartTime % 60000) / 1000;

        //Redundancy to kill any lingering alerts
            setHue(BULB1,false); //Set Hue bulb1 to off
            setHue(BULB2,false); //Set Hue bulb2 to off
            //wemoWrite(WEMO_STOVE, LOW); //doesn't mess with stove unless you want it to!
            wemoWrite(WEMO_RADIO, LOW);

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

    //--------LOW TEMP ALERT SCREEN Shows temp, and alert message-------//

        display.clearDisplay();

        // Read TC here for most up-to-date value before display
        // Start timer for TC readings for every half-second
       updateTemperature();

        //Hue and Wemo alerts for low temp
        if (tempF < tempLow) {
             setHue(BULB1,true,HueRed,255,255); //Set Hue bulb to red for alert
            wemoWrite(WEMO_RADIO, HIGH);
               
        }   

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

       // Read TC here for most up-to-date value before display
        // Start timer for TC readings for every half-second
        updateTemperature();

        //Hue and Wemo alerts for high temp
        if (tempF > tempHigh) {
             setHue(BULB1,true,HueBlue,255,255); //Set Hue bulb to blue for alert
                wemoWrite(WEMO_STOVE, LOW); //turn off stove
                wemoWrite(WEMO_RADIO, HIGH); //turn on radio 
        }

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

        cancelWaitTimer.startTimer(cancelWaitMsec); //start the timer for how long the cancel screen is held

        // Read TC here for most up-to-date value before display
        // Start timer for TC readings for every half-second
        updateTemperature();

        //  Cancel all alerts, reset wemos and turn off hue bulbs
        setHue(BULB1,false); //Set Hue bulb1 to off
        setHue(BULB2,false); //Set Hue bulb2 to off
        wemoWrite(WEMO_STOVE, LOW);
        wemoWrite(WEMO_RADIO, LOW);

        //  Display cancel screen, then monitor screen after a few seconds to give time for processes to cancel
        display.clearDisplay();
        display.drawBitmap(0, 0, BITMAP_CANCEL, 40, 40, WHITE); // Cancel graphic
        display.setTextSize(1);
        display.setCursor(0, 45);
        display.printf("Cancelling... Please wait.");
        display.display();
    
      
        }


 

