#include "MAX6675_RK.h"

void MAX6675_RK::setup() {
    spi->begin(csPin);
}

uint16_t MAX6675_RK::readRaw() {
    uint16_t result;

    spi->beginTransaction(spiSettings);
    digitalWrite(csPin, LOW);

    uint8_t b[2];
    b[0] = spi->transfer(0x00);
    b[1] = spi->transfer(0x00);

    result = (b[0] << 8) | b[1];

    digitalWrite(csPin, HIGH);
    spi->endTransaction();

    return result;
}

float MAX6675_RK::readValue() {
    uint16_t raw = readRaw();

    if (openDetection) {
        if ((raw & 0b100) != 0) {
            // Open or missing sensor
            return nanf("");
        }
    }

    // Remove low 3 bits
    raw >>= 3;

    // Result is in quarter degree increments
    return ((float)raw) / 4.0;
}
