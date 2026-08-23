#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>

enum EncoderEventType
{
    ENCODER_NONE,
    ENCODER_PULSE,
    ENCODER_PRESS
};


struct EncoderEvent
{
    EncoderEventType type = ENCODER_NONE;

    int value = 0;
};


class Encoder
{
public:

    void begin(
        int pinA,
        int pinB,
        int buttonPin
    );

    void update();


    bool available();

    EncoderEvent read();

    // --------------------------------------------------------
    // Test / event injection
    // --------------------------------------------------------

    void injectPulse(
        int value
    );    

private:

    int pinA = -1;
    int pinB = -1;
    int buttonPin = -1;


    EncoderEvent lastEvent;
//nu kan input ook zijn llllll
//of lllr
//of rrrrrr

    // --------------------------------------------------------
    // Rotary encoder
    // --------------------------------------------------------

    uint8_t lastEncoderState = 0;

    int encoderAccumulator = 0;


    // --------------------------------------------------------
    // Encoder button
    // --------------------------------------------------------

    static constexpr unsigned long BUTTON_DEBOUNCE_TIME = 10;

    bool stableButtonState = HIGH;

    bool candidateButtonState = HIGH;

    unsigned long candidateButtonSince = 0;
};


#endif