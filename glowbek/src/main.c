// Glowbek: The doumbek that glows!
// Author: Daniel Tullemans / "LuminousElement"
// Created 2014

#include "light_ws2812.h"
#include <string.h> /* memset */
#include <stdlib.h> /* rand */
#include <util/delay.h>
#include <avr/io.h>
#include "hsv.h"
#include "pulse.h"

typedef struct cRGB cRGB;

void setupAdcOnPin(uint8_t pin) {
    // TODO: assert pin <= 7

    DDRC &= 0xC0;  // Set ADC pins as inputs
    PORTC &= 0xC0; // Set ADC pins to not use internal pull-up
    DIDR0 &= 0x3F; // Disable digital input buffers for all ADC pins
    ADCSRA |= _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0); // Set ADC prescaler to 128 -- 125KHz sample rate @ 16MHz

    ADMUX  = 0x7 & pin; // Set ADC multiplexer to read from the given channel

    // ADMUX:   REFS1   REFS0   Refernce
    //          0       0       External ARef
    //          0       1       Vcc
    //          1       1       Internal 1.1V (ATMega168/328) / 2.56V (ATMega8)

    ADMUX |= /*_BV(REFS0) |*/ _BV(REFS1);
    ADMUX |= _BV(ADLAR); // Make the ADC left-align the values. We then read bits 9 to 2 from ADCH (ignoring bits 0 and 1 in ADCL).

    ADCSRA |= _BV(ADEN); // Power up the ADC
}

inline void adcStartConversion() {
    ADCSRA |= _BV(ADSC); // Start ADC measurements
}

inline void adcWaitReady()
{
    // NB: Reading ADCH assumes that we started ADC conversion at least 192
    //     clocks ago (or 1,728 clocks if it was the first conversion after
    //     powering up the ADC. If we can't be sure of that, we should wait for
    //     the ADIF bit in ADCSRA to be set.
	while (!(ADCSRA & _BV(ADIF)));
}

uint8_t findPeak(uint8_t values[], uint8_t start, uint8_t length)
{
    uint8_t peak = 0;

    for (int i = 0; i < length; ++i)
    {
        uint8_t val = values[start + i];
        peak = val > peak ? val : peak;
    }
    return peak;
}

#define NUM_LEDS 40
cRGB frameBuffer[NUM_LEDS];

#define WINDOW_SIZE 13
#define HISTORY_COUNT 50
#define WINDOW_START (HISTORY_COUNT - WINDOW_SIZE)
#define HYSTERESIS 50

uint8_t isPulseDetected(uint8_t historicalPeak, uint8_t currentPeak)
{
    return currentPeak > historicalPeak 
        && (currentPeak - historicalPeak) > HYSTERESIS;
}

void debugHistoryBuffer(uint8_t historicalPeak, uint8_t currentPeak, uint8_t sampleHistory[])
{
    frameBuffer[0].r = historicalPeak;
    frameBuffer[1].g = currentPeak;

    frameBuffer[38].r = 
    frameBuffer[38].g = 
    frameBuffer[38].b = 
        isPulseDetected(historicalPeak, currentPeak) ? 255 : 0;
    

    for (int i = 0; i < (NUM_LEDS - 5); ++i)
    {
        frameBuffer[i + 3].b = sampleHistory[i];
    }
}

int main()
{
    /////////////////////////////
    // Set up the analogue input
    setupAdcOnPin(0);

    adcStartConversion(); // Start ADC measurement

    pulseSetFrameBuffer(frameBuffer, NUM_LEDS);

    const uint8_t pulseCount = 10;
    uint8_t pulseIdx = 0;
    struct cPulse pulses[pulseCount];
    memset(pulses, 0, sizeof(struct cPulse) * pulseCount);

    for (int i = 0; i < pulseCount; i += 2)
    {
        pulses[i].direction = 1;
    }

    uint8_t sampleHistory[HISTORY_COUNT];
    memset(sampleHistory, 0, HISTORY_COUNT);

    int8_t startingPosition = 0;

    while(1)
    {
        // Hist = = = = =
        // Wind       - -
        // Copy   - -
        for (int i = 0; i < (HISTORY_COUNT - WINDOW_SIZE); ++i)
        {
            sampleHistory[i] = sampleHistory[i + WINDOW_SIZE];
        }

        for (int i = 0; i < WINDOW_SIZE; ++i)
        {
            adcStartConversion();
            _delay_us(500);
            adcWaitReady();
            sampleHistory[WINDOW_START + i] = ADCH;
        }

        uint8_t historicalPeak = findPeak(sampleHistory, 0, WINDOW_START);
        uint8_t currentPeak = findPeak(sampleHistory, WINDOW_START, WINDOW_SIZE);

        if (isPulseDetected(historicalPeak, currentPeak))
        {
            // Generate a new pulse
            if (++pulseIdx >= pulseCount) pulseIdx = 0;

            pulses[pulseIdx].colour.h = rand() % MAX_HUE;
            pulses[pulseIdx].colour.s = (MAX_SAT / 2) + (rand() % (MAX_SAT / 2));
            pulses[pulseIdx].colour.v = MAX_VAL;
            pulses[pulseIdx].position = startingPosition;
            startingPosition -= 3;
            if (startingPosition < 0) startingPosition += NUM_LEDS;
        }

        pulseClearFrameBuffer();

        for (int i = 0; i < pulseCount; ++i)
        {
            pulseUpdate(pulses + i);
            pulseRender(pulses + i);
        }

        ws2812_setleds(frameBuffer, 40); // Blocks for ~1.2ms
        _delay_ms(2);         // The sensor line will be noisy for a little while
    }
}
