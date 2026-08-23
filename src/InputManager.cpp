#include <Arduino.h>

#include "InputManager.h"


// ============================================================
// BEGIN
// ============================================================

void InputManager::begin(
    ButtonMatrix& matrix,
    Encoder& encoder
)
{
    this->matrix =
        &matrix;

    this->encoder =
        &encoder;


    lastEvent.type =
        EVENT_NONE;

    lastEvent.value =
        0;


    // --------------------------------------------------------
    // Button state
    // --------------------------------------------------------

    stableKey =
        -1;

    candidateKey =
        -1;

    candidateSince =
        0;
}



// ============================================================
// UPDATE
// ============================================================

void InputManager::update()
{
    lastEvent.type =
        EVENT_NONE;

    lastEvent.value =
        0;


    // ========================================================
    // ENCODER
    // ========================================================

    if(encoder != nullptr)
    {
        encoder->update();

        if(encoder->available())
        {
            EncoderEvent event =
                encoder->read();

            lastEvent =
                encoderEvent(event);

            return;
        }
    }


    // ========================================================
    // BUTTON MATRIX
    // ========================================================

    if(matrix == nullptr)
        return;


    matrix->update();

    int currentKey =
        matrix->currentKey();

    /*
        Fysieke toestand is veranderd.

        Start opnieuw met debouncen.
    */

    if(currentKey != candidateKey)
    {
        candidateKey =
            currentKey;

        candidateSince =
            millis();

        return;
    }


    /*
        Toestand moet DEBOUNCE_TIME
        stabiel blijven.
    */

    if(
        millis() - candidateSince <
        DEBOUNCE_TIME
    )
    {
        return;
    }


    /*
        Toestand is stabiel, maar is niet
        veranderd ten opzichte van de
        laatst geaccepteerde toestand.
    */

    if(candidateKey == stableKey)
        return;


    /*
        Nieuwe toestand accepteren.
    */

    stableKey =
        candidateKey;


    /*
        Alleen een overgang van
        geen toets → toets genereert
        een event.
    */

    if(stableKey != -1)
    {
        lastEvent =
            buttonEvent(stableKey);


        // ----------------------------------------------------
        // DEBUG
        // ----------------------------------------------------

        Serial.print(
            "[InputManager] Key event: "
        );

        Serial.print(
            stableKey
        );

        Serial.print(
            " -> event type "
        );

        Serial.println(
            lastEvent.type
        );
    }
}



// ============================================================
// BUTTON EVENT
// ============================================================

Event InputManager::buttonEvent(
    int key
)
{
    Event event;


    event.type =
        EVENT_NONE;

    event.value =
        0;


    switch(key)
    {
        case 1:

            event.type =
                EVENT_KEY_1;

            break;


        case 2:

            event.type =
                EVENT_KEY_2;

            break;


        case 3:

            event.type =
                EVENT_KEY_3;

            break;


        case 4:

            event.type =
                EVENT_KEY_4;

            break;


        case 5:

            event.type =
                EVENT_KEY_5;

            break;


        case 6:

            event.type =
                EVENT_KEY_6;

            break;


        case 7:

            event.type =
                EVENT_KEY_7;

            break;


        case 8:

            event.type =
                EVENT_KEY_8;

            break;


        case 9:

            event.type =
                EVENT_KEY_9;

            break;
    }


    return event;
}



// ============================================================
// ENCODER EVENT
// ============================================================

Event InputManager::encoderEvent(
    EncoderEvent event
)
{
    Event result;


    result.type =
        EVENT_NONE;

    result.value =
        0;


    switch(event.type)
    {
        case ENCODER_PULSE:

            result.type =
                EVENT_ENCODER_PULSE;

            result.value =
                event.value;

            break;


        case ENCODER_PRESS:

            result.type =
                EVENT_ENCODER_PRESS;

            break;


        case ENCODER_NONE:

            break;
    }


    return result;
}



// ============================================================
// AVAILABLE
// ============================================================

bool InputManager::available()
{
    return lastEvent.type != EVENT_NONE;
}



// ============================================================
// READ
// ============================================================

Event InputManager::read()
{
    Event result =
        lastEvent;


    lastEvent.type =
        EVENT_NONE;

    lastEvent.value =
        0;


    return result;
}