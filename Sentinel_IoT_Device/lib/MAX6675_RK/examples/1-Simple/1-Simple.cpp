#include "Particle.h"

#include "MAX6675_RK.h"

SerialLogHandler logHandler(LOG_LEVEL_INFO);

SYSTEM_MODE(SEMI_AUTOMATIC);

#ifndef SYSTEM_VERSION_v620
SYSTEM_THREAD(ENABLED); // System thread defaults to on in 6.2.0 and later and this line is not required
#endif

MAX6675_RK sensor;
// MAX6675_RK sensor(A0);
// MAX6675_RK sensor(&SPI1, D5);

unsigned long lastSampleMs = 0;
std::chrono::milliseconds samplePeriod = 2s;

void setup() {
    sensor.setup();

    Particle.connect();
}

void loop() {
    if (millis() - lastSampleMs >= samplePeriod.count()) {
        lastSampleMs = millis();

        float value = sensor.readValue();
        if (!std::isnan(value)) {
            Log.info("value=%f", value);
        }
        else {
            Log.info("sensor not available");
        }
    }
}

/* Example output:

0000022622 [app] INFO: value=26.500000
0000024622 [app] INFO: value=26.750000
0000026622 [app] INFO: value=26.500000
0000028622 [app] INFO: value=26.500000
0000030622 [app] INFO: value=26.500000
0000032622 [app] INFO: value=26.250000
0000034622 [app] INFO: value=26.500000
0000036622 [app] INFO: value=26.500000
0000038622 [app] INFO: value=26.750000

 */