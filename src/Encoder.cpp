#include "Encoder.h"

#include <Arduino.h>


// ============================================================
// BEGIN
// ============================================================

void Encoder::begin(
    int pinA,
    int pinB,
    int buttonPin
)
{
    this->pinA =
        pinA;

    this->pinB =
        pinB;

    this->buttonPin =
        buttonPin;


    pinMode(
        this->pinA,
        INPUT_PULLUP
    );

    pinMode(
        this->pinB,
        INPUT_PULLUP
    );

    pinMode(
        this->buttonPin,
        INPUT_PULLUP
    );


    lastEvent.type =
        ENCODER_NONE;

    lastEvent.value =
        0;


    // --------------------------------------------------------
    // Encoder initial state
    // --------------------------------------------------------

    lastEncoderState =
        (digitalRead(this->pinA) << 1) |
         digitalRead(this->pinB);


    encoderAccumulator =
        0;


    // --------------------------------------------------------
    // Button initial state
    // --------------------------------------------------------

    stableButtonState =
        digitalRead(this->buttonPin);

    candidateButtonState =
        stableButtonState;

    candidateButtonSince =
        millis();
}



// ============================================================
// UPDATE
// ============================================================

void Encoder::update()
{
    /*
        Eén event maximaal per update.

        Als er al een event klaarstaat, wachten we
        totdat dit event door read() is opgehaald.
    */

    if(lastEvent.type != ENCODER_NONE)
        return;


    // ========================================================
    // ROTARY ENCODER
    // ========================================================

    uint8_t currentState =
        (digitalRead(pinA) << 1) |
         digitalRead(pinB);


    if(currentState != lastEncoderState)
    {
        /*
            Quadrature transition table.

            Index:
                previous state << 2 | current state

            Geldige overgang:
                0001, 0111, 1110, 1000 = +1

            Tegengestelde richting:
                0010, 1011, 1101, 0100 = -1
        */

        static const int8_t transitionTable[16] =
        {
             0, -1,  1,  0,
             1,  0,  0, -1,
            -1,  0,  0,  1,
             0,  1, -1,  0
        };


        uint8_t transition =
            (lastEncoderState << 2) |
            currentState;


        encoderAccumulator +=
            transitionTable[transition];


        lastEncoderState =
            currentState;


        /*
            Een volledige quadrature cyclus bestaat uit
            vier geldige transities.

            We rapporteren daarom één pulse per volledige
            encoderstap.
        */

        if(encoderAccumulator >= 4)
        {
            encoderAccumulator =
                0;

            lastEvent.type =
                ENCODER_PULSE;

            lastEvent.value =
                +1;

            return;
        }


        if(encoderAccumulator <= -4)
        {
            encoderAccumulator =
                0;

            lastEvent.type =
                ENCODER_PULSE;

            lastEvent.value =
                -1;

            return;
        }
    }


    // ========================================================
    // ENCODER BUTTON
    // ========================================================

    bool currentButtonState =
        digitalRead(buttonPin);


    /*
        Fysieke toestand veranderd:
        start opnieuw met debouncen.
    */

    if(currentButtonState != candidateButtonState)
    {
        candidateButtonState =
            currentButtonState;

        candidateButtonSince =
            millis();

        return;
    }


    /*
        Toestand moet lang genoeg stabiel zijn.
    */

    if(
        millis() - candidateButtonSince <
        BUTTON_DEBOUNCE_TIME
    )
    {
        return;
    }


    /*
        Toestand is stabiel en anders dan
        de laatst geaccepteerde toestand.
    */

    if(candidateButtonState == stableButtonState)
        return;


    stableButtonState =
        candidateButtonState;


    /*
        Alleen indrukken is een event.
        Loslaten niet.
    */

    if(stableButtonState == LOW)
    {
        lastEvent.type =
            ENCODER_PRESS;

        lastEvent.value =
            0;

        return;
    }
}

// ============================================================
// INJECT PULSE
// ============================================================

void Encoder::injectPulse(
    int value
)
{
    if(value == 0)
        return;


    lastEvent.type =
        ENCODER_PULSE;

    lastEvent.value =
        value;
}

// ============================================================
// AVAILABLE
// ============================================================

bool Encoder::available()
{
    return lastEvent.type != ENCODER_NONE;
}



// ============================================================
// READ
// ============================================================

EncoderEvent Encoder::read()
{
    EncoderEvent event =
        lastEvent;


    lastEvent.type =
        ENCODER_NONE;

    lastEvent.value =
        0;


    return event;
}