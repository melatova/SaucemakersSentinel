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

//#include <math.h> 

SYSTEM_MODE(SEMI_AUTOMATIC);

// Define using SPI1 (D2-D4) and D2 as the Chip Select
MAX6675_RK thermocouple(&SPI1, D2);


// Calibration offset (Adjust this after boiling water test)
float calibrationOffset = 0.0; //tested with boiling water, no offset needed. retest often.

void setup() {
    Serial.begin(9600);
    //thermocouple.withOpenDetection(false); // Add this line to disable open detection if needed
    // RK's library includes an SPI.begin() and pinMode setup
    thermocouple.setup();
    
    Serial.println("--- MAX6675_RK Hardware SPI Test ---");
}

void loop() {

    // use RK's function, readValue()
    float rawC = thermocouple.readValue();
    float finalC = rawC + calibrationOffset;

    if (std::isnan(rawC)) {
        Serial.println("SENSOR ERROR: Check if probe is screwed in tight!");
    } else if (rawC == 0.0) {
        Serial.println("CONNECTION ERROR: Check D2, D3, D4 wiring!");
    } else {
        // Serial.print("Temp: ");
        // Serial.print(finalC);
        // Serial.println(" C");
    

    //delay(1000); 

    float total = 0;
    for(int i = 0; i < 4; i++) {
       total =total + thermocouple.readValue();
    delay(200); // small wait for the MAX6675 to calculate and convert the reading
    }
    float averageTemp = total / 4.0;
        Serial.printf("Avg Temp: %.2f C\n", averageTemp);
    }
 }
