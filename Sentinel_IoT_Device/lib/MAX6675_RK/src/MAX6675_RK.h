#ifndef __MAX6675_RK_H
#define __MAX6675_RK_H

#include "Particle.h"

// Repository: https://github.com/rickkas7/MAX6675_RK
// License: MIT

/**
 * @brief Class for reading a MAX6665 SPI thermocouple interface
 */
class MAX6675_RK {
public:
    /**
     * @brief Construct an object using primary SPI (SPI) and default CS pin (SS)
     */
    MAX6675_RK() {};

    /**
     * @brief Construct an object using primary SPI (SPI) and the specified CS pin
     * 
     * @param csPin A pin, such as D2.
     */
    MAX6675_RK(pin_t csPin) : csPin(csPin) {};

    /**
     * @brief Construct an object using the specified SPI interface and the specified CS pin
     * 
     * @param spi A SPI interface such as &SPI or &SPI1.
     * @param csPin A pin, such as D2.
     */
    MAX6675_RK(SPIClass *spi, pin_t csPin) : spi(spi), csPin(csPin) {};

    /**
     * @brief Set the SPI interface if not setting it from the constructor
     * 
     * @param spi A SPI interface such as &SPI or &SPI1.
     * @return MAX6675_RK& 
     * 
     * Must be called before setup().
     */
    MAX6675_RK &withSPI(SPIClass *spi) { this->spi = spi; return *this; };

    /**
     * @brief Set the CS pin it not setting it from the constructor
     * 
     * @param csPin A pin, such as D2.
     * @return MAX6675_RK& 
     * 
     * Must be called before setup().
     */
    MAX6675_RK &withCS(pin_t csPin) { this->csPin = csPin; return *this; };

    /**
     * @brief Sets whether open detection mode is enabled or not (default is true)
     * 
     * @param value true is enable open detection, false to disable
     * @return MAX6675_RK& 
     * 
     * The MAX6675 can detect sensor open (or missing), but only if T- and GND
     * are connected. This feature is enabled by default because the suggested
     * design connects the two. If for some reason you've isolated T- and GND,
     * then be sure to use withOpenDetection(false).
     */
    MAX6675_RK &withOpenDetection(bool value) { openDetection = value; return *this; };

    /**
     * @brief Perform setup operations; call this from global application setup()
     * 
     * This must be done after with withXXX() methods. It basically calls spi->begin()
     * with the specified csPin. This sets the pinMode of the SPI pins and csPin, 
     * and also sets the csPin to HIGH.
     */
    void setup();

    /**
     * @brief Read the raw 16-bit value from the sensor
     * 
     * @return uint16_t 
     */
    uint16_t readRaw();

    /**
     * @brief Return a value in degrees Celsius
     * 
     * @return float A value in degrees C. It will be in the range 0 <= value <= 1023.75.
     * 
     * Returns NaN if the sensor (T+ or T-) is not connected. See withOpenDetection().
     * 
     * Will typically return 0.0 if the chip is not connected via SPI.
     */
    float readValue();

    /**
     * @brief Convert a value from degrees C to degrees F.
     * 
     * @param value 
     * @return float 
     */
    static float convertCtoF(float value) { return value * 9.0 / 5.0 + 32.0; };

    /**
     * @brief Return a value in degrees Fahrenheit 
     * 
     * @return float A value in degrees F. It will be in the range of 32.0 <= value <= 1874.75.
     * 
     * Returns NaN if the sensor (T+ or T-) is not connected. See withOpenDetection().
     * 
     * Will typically return 32.0 if the chip is not connected via SPI.
     */
    float readValueF() { return convertCtoF(readValue()); };



protected:
    SPIClass *spi = &SPI; //!< SPI interface to use 
    pin_t csPin = SS; //!< Pin to use for CS
    bool openDetection = true; //!< Whether to detect open or missinbg sensor

    /**
     * @brief SPI settings, You won't generally have to change these settings; they're based on what the MAX6675 requires.
     */
    SPISettings spiSettings = SPISettings(4 * MHZ, MSBFIRST, SPI_MODE3);


};
#endif  /* __MAX6675_RK_H */
