#include "Particle.h"

#include "MAX6675_RK.h"

SerialLogHandler logHandler(LOG_LEVEL_INFO);

SYSTEM_MODE(SEMI_AUTOMATIC);

#ifndef SYSTEM_VERSION_v620
SYSTEM_THREAD(ENABLED); // System thread defaults to on in 6.2.0 and later and this line is not required
#endif

const pin_t sensorPins[] = {
    S3, D3
};
const size_t numSensorsPins = sizeof(sensorPins) / sizeof(sensorPins[0]);

MAX6675_RK sensors[numSensorsPins];

unsigned long lastSampleMs = 0;
std::chrono::milliseconds samplePeriod = 2s;

void setup() {
    for(size_t ii = 0; ii < numSensorsPins; ii++) {
        sensors[ii]
            .withCS(sensorPins[ii])
            .setup();
    }

    Particle.connect();
}

void loop() {
    if (millis() - lastSampleMs >= samplePeriod.count()) {
        lastSampleMs = millis();

        for(size_t ii = 0; ii < numSensorsPins; ii++) {

            float value = sensors[ii].readValue();
            if (!std::isnan(value)) {
                Log.info("%d: %.2f C, %.2f F", (int)ii, value, MAX6675_RK::convertCtoF(value));
            }
            else {
                Log.info("%d: sensor not available", (int)ii);
            }
        }
    }
}
