// pulse.c
#include "pulse.h"
#include <string.h> /* memset */
#include "rgb.h"

struct cRGB *fBuf;
uint8_t fbLength;

void pulseSetFrameBuffer(struct cRGB bufferToUse[], uint8_t length)
{
    fBuf = bufferToUse;
    fbLength = length;
}

void pulseClearFrameBuffer()
{
    memset(fBuf, 0, 3 * fbLength);
}

void pulseUpdate(struct cPulse *pulse)
{
    hsvDarkenFalloff(&(pulse->colour));

    pulse->position += (pulse->direction) ? 1 : -1;
    if (pulse->position == fbLength)
    {
        pulse->position = 0;
    }
}

void pulseRender(struct cPulse *pulse)
{
    // Disable early return because constant framerate
    // Don't bother if the pulse is black. Black + black =.... well, you know.
    //if (pulse->colour.v == 0) return;

    // Start at the pulse's position and work backward
    for (int8_t i = 0; i < 3; ++i)
    {
        int8_t pixelIdx = (pulse->position - i);
        if (pixelIdx < 0) pixelIdx += fbLength; // wrap around

        struct cRGB *pixel = fBuf + pixelIdx;

        struct cRGB colourToAdd = hsvToRgb(&(pulse->colour));

        rgbAdd(pixel, &colourToAdd);
    }
}
