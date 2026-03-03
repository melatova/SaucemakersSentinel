/* 
 * Project: Saucemakers_Sentinel__a_temp_monitor
 * Author: Nicole
 * Date: 03/02/2026
 * Description: Keep your bottling temp in range! 
 * Temp is sensed, monitored, and displayed, 
 * while the timer counts down. 
 * You're alerted if your temp goes awry!
 */

// Include Particle Device OS APIs
#include "Particle.h"
#include "MAX6675_RK.h"
#include "Adafruit_BME280.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "neopixel.h"
#include "IoTTimer.h"
#include "Button.h"
#include "bitmaps.h"
#include "wemo.h"   

//#include <math.h> 

SYSTEM_MODE(MANUAL); //control logging into classroom router);
SYSTEM_THREAD(DISABLED);

//Defining the thermocouple object
MAX6675_RK thermocouple(&SPI1, D2); // Define using SPI1 (D2-D4) and D2 as the Chip Select as it's not the default in RK's library. You can change this to use the default SPI and CS pin if you prefer. See the MAX6675_RK class for details.
// Thermocouple calibration offset (Adjust after doing a boiling water test)
float calibrationOffset = 0.0; //tested with boiling water, no offset needed. retest often.

//Defining the I2C sensor object
Adafruit_BME280 bme;

//Defining OLED display object
const int OLED_RESET=-1;
Adafruit_SSD1306 display(OLED_RESET);

//Defining the NeoPixel object
const int PIXELCOUNT = 1;
Adafruit_NeoPixel pixel ( PIXELCOUNT , SPI1 , WS2812B );

//Defining the Wemo and Hue objects
const int STOVEWEMO=3; //Which wemo to control
const int RADIOWEMO=4; //Which wemo to control
bool onOFF = false; // internal state variable to track whether wemo is on or off

//Variables, constants, objects, custom functions
const int hexAddress = 0x76; //address for the BME
bool status; //variable to make sure BME is working
float tempC, tempF, pressPA, pressInHg, humidRH; //variables for BME280 readings, stored for current and future features
IoTTimer timer; //declare object for the IoTTimer
unsigned int msec = 500; //variable for the IoTTimer
Button greenstartButton(D17); //make a button object for the yellow button connected to D17
float finalF; //variable to store calculated farenheit temp
float tempHigh = 200.00; //high temp threshold for alerts, in F
float tempLow = 185.00; //low temp threshold for alerts, in F
//HSV currentHsv = {0.59, 1.00, 0.33}; //initialize with a color, variable for storage
const int blue = 0x0000FF; //cool
const int green = 0x00FF00; //comfortable
const int yellow = 0xFFFF00; //warm
const int red = 0xFF0000; //hot

void setup() {

    //Initiate serial monitor
    Serial.begin(9600);

    //Initiate Thermocouple
    //thermocouple.withOpenDetection(false); // uncomment to disable open detection if needed
    thermocouple.setup();
    //Serial.println("--- MAX6675_RK Hardware SPI Test ---"); //uncomment for testing thermocouple wiring

    //initiate the BME280 (environmental sensor)
    status = bme.begin(hexAddress);
            if (status == false){
                Serial.printf("BME at address0x%02X failed to start", hexAddress);
            }

    // initiate the IoTTimer
    timer.startTimer(msec);

    // initialize the OLED display
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.setTextColor(WHITE);
    display.clearDisplay();
    display.setTextSize(1);

    

    // initialize the neoPixel 
    pixel.begin();
    pixel.setBrightness(25); // set brightness to 50 so it's mellow
    pixel.setPixelColor(0, blue); // start with blue for cool temp
    pixel.show();

    // Initialize the wemo - have it connect to wifi, clear any previous credentials, and set it to our router, IoTNetwork
    pinMode(D7, OUTPUT); // set D7 as the button to turn on and off the wemo, outputs a click.
    WiFi.on();
    WiFi.clearCredentials();           // clear current credentials of the wemo
    WiFi.setCredentials("IoTNetwork"); // set the credentials to our network
    WiFi.connect();

    //Initialize HUE bulbs

}

void loop() {


    //Thermocouple reading
    // use RK's function, readValue()
    float rawC = thermocouple.readValue();
    float finalC = rawC + calibrationOffset;

    if (std::isnan(rawC))
    {
        Serial.println("SENSOR ERROR: Check if probe is screwed in tight!");
    }
    else if (rawC == 0.0)
    {
        Serial.println("CONNECTION ERROR: Check D2, D3, D4 wiring!");
    }
    else
    {

    float total = 0;
        for (int i = 0; i < 4; i++)
        {
            total = total + thermocouple.readValue();
            delay(200); // small wait for the MAX6675 to calculate and convert the reading
        }
        float averageTemp = total / 4.0;
        Serial.printf("Avg Temp: %.2f C\n", averageTemp);
    }
    
    //Convert to F 
    float finalF = finalC * 1.8 + 32;
    Serial.printf("Current Temp: %.2f F\n", finalF);

    //Show the temp on the OLED display
    //display.clearDisplay();
    display.drawBitmap(0, 0, BITMAP_TEMP_F, 40, 40, WHITE); //temp icon 
    display.setCursor(5, 42);
    display.print(finalF); //temp reading in F
    display.print(" F");
    display.display();

    }

 

