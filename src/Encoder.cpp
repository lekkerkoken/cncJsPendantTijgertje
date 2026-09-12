#include "Encoder.h"


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
        pinA,
        INPUT_PULLUP
    );

    pinMode(
        pinB,
        INPUT_PULLUP
    );

    /*
        External 10k pull-up to 3.3V.

        Button:
        HIGH = pressed
        LOW  = released
    */

    pinMode(
        buttonPin,
        INPUT_PULLDOWN
    );


    // ========================================================
    // EVENT QUEUE
    // ========================================================

    eventQueue =
        xQueueCreate(
            EVENT_QUEUE_LENGTH,
            sizeof(EncoderEvent)
        );


    // ========================================================
    // ROTARY ENCODER
    // ========================================================
    delay(100);
    lastEncoderState =
        (
            (digitalRead(pinA) << 1) |
            digitalRead(pinB)
        );



    portENTER_CRITICAL(
        &encoderMux
    );

    encoderAccumulator =
        0;

    buttonChanged =
        false;

    portEXIT_CRITICAL(
        &encoderMux
    );


    // ========================================================
    // BUTTON
    // ========================================================

    if(
        digitalRead(buttonPin) ==
        LOW
    )
    {
        buttonState =
            BUTTON_DEBOUNCING_PRESS;
    }
    else
    {
        buttonState =
            BUTTON_RELEASED;
    }


    buttonStateSince =
        millis();


    // ========================================================
    // INTERRUPTS
    // ========================================================

    attachInterruptArg(
        pinA,
        Encoder::encoderISR,
        this,
        CHANGE
    );

    attachInterruptArg(
        pinB,
        Encoder::encoderISR,
        this,
        CHANGE
    );

    attachInterruptArg(
        buttonPin,
        Encoder::buttonISR,
        this,
        CHANGE
    );


    // ========================================================
    // FREERTOS TASK
    // ========================================================

    xTaskCreate(
        Encoder::taskEntry,
        "Encoder",
        TASK_STACK_SIZE,
        this,
        TASK_PRIORITY,
        &taskHandle
    );
}


// ============================================================
// UPDATE
// ============================================================

void Encoder::update()
{
    /*
        Compatibility function.

        Encoder processing is handled by
        the FreeRTOS task.
    */
}


// ============================================================
// ENCODER ISR
// ============================================================

void ARDUINO_ISR_ATTR Encoder::encoderISR(
    void* parameter
)
{
    Encoder* encoder =
        static_cast<Encoder*>(parameter);


    if(
        encoder ==
        nullptr
    )
    {
        return;
    }


    encoder->handleEncoderTransition();
}


// ============================================================
// BUTTON ISR
// ============================================================

void ARDUINO_ISR_ATTR Encoder::buttonISR(
    void* parameter
)
{
    Encoder* encoder =
        static_cast<Encoder*>(parameter);


    if(
        encoder ==
        nullptr
    )
    {
        return;
    }


    /*
        ISR doet uitsluitend signaleren
        dat de GPIO veranderd is.

        Debouncing en timing gebeuren
        buiten de ISR.
    */

    portENTER_CRITICAL_ISR(
        &encoder->encoderMux
    );

    encoder->buttonChanged =
        true;

    portEXIT_CRITICAL_ISR(
        &encoder->encoderMux
    );
}


// ============================================================
// ENCODER TRANSITION
// ============================================================

// ============================================================
// ENCODER TRANSITION - DIAGNOSTIC TEST
// ============================================================

void Encoder::handleEncoderTransition()
{
    static const int8_t transitionTable[16] =
    {
         0, -1,  1,  0,
         1,  0,  0, -1,
        -1,  0,  0,  1,
         0,  1, -1,  0
    };


    const uint8_t previousState =
        lastEncoderState;


    const uint8_t currentState =
        (
            (digitalRead(pinA) << 1) |
            digitalRead(pinB)
        );


    const uint8_t index =
        (
            (previousState << 2) |
            currentState
        );


    const int8_t delta =
        transitionTable[index];


    lastEncoderState =
        currentState;


    if(
        delta != 0
    )
    {
        portENTER_CRITICAL_ISR(
            &encoderMux
        );

        encoderAccumulator +=
            delta;

        portEXIT_CRITICAL_ISR(
            &encoderMux
        );


        /*
            DIAGNOSTIEK

            LET OP:
            Serial vanuit een ISR is normaal gesproken
            niet ideaal.

            Dit is alleen voor deze korte test.
        */
       
    }
}

// ============================================================
// TASK ENTRY
// ============================================================

void Encoder::taskEntry(
    void* parameter
)
{
    Encoder* encoder =
        static_cast<Encoder*>(parameter);


    if(
        encoder !=
        nullptr
    )
    {
        encoder->task();
    }


    vTaskDelete(
        nullptr
    );
}


// ============================================================
// TASK
// ============================================================

void Encoder::task()
{
    for(;;)
    {
        // ====================================================
        // ENCODER
        // ====================================================

        int delta =
            0;


        portENTER_CRITICAL(
            &encoderMux
        );


        /*
            Test 1:

            Vier quadrature-counts in dezelfde richting
            vormen één fysieke encoder-pulse.

            Belangrijk:

            We verwijderen alleen de vier gebruikte counts.
            Een eventuele resterende accumulator blijft dus
            behouden.

            Voorbeeld:

                +1 +1 +1 +1
                -> pulse +1
                -> accumulator 0
        */

if(
    encoderAccumulator >=
    2
)
{
    delta =
        1 * (encoderAccumulator / 2);

    encoderAccumulator -=
        encoderAccumulator / 2 * 2;
}
else if(
    encoderAccumulator <=
    -2
)
{
    delta =
        1 * (encoderAccumulator / 2);

    encoderAccumulator -=
        encoderAccumulator / 2 * 2;
}

        portEXIT_CRITICAL(
            &encoderMux
        );


        // ====================================================
        // ENCODER EVENT
        // ====================================================

        if(
            delta !=
            0
        )
        {
            EncoderEvent event;

            event.type =
                ENCODER_PULSE;

            event.value =
                delta;


            xQueueSend(
                eventQueue,
                &event,
                0
            );


#ifdef ENCODER_DEBUG

            Serial.print(
                "[Encoder] Pulse: "
            );

            Serial.println(
                delta
            );

#endif
        }


        // ====================================================
        // BUTTON CHANGE FLAG
        // ====================================================

        bool changed =
            false;


        portENTER_CRITICAL(
            &encoderMux
        );

        changed =
            buttonChanged;

        buttonChanged =
            false;

        portEXIT_CRITICAL(
            &encoderMux
        );


        // ====================================================
        // BUTTON
        // ====================================================

        updateButton();


        // ====================================================
        // TASK DELAY
        // ====================================================

        vTaskDelay(
            pdMS_TO_TICKS(
                TASK_DELAY_MS
            )
        );
    }
}


// ============================================================
// BUTTON STATE MACHINE
// ============================================================

void Encoder::updateButton()
{
    const bool pressed =
        digitalRead(buttonPin) ==
        HIGH;


    const unsigned long now =
        millis();


    switch(
        buttonState
    )
    {
        // ====================================================
        // RELEASED
        // ====================================================

        case BUTTON_RELEASED:

            if(
                pressed
            )
            {
                buttonState =
                    BUTTON_DEBOUNCING_PRESS;

                buttonStateSince =
                    now;


#ifdef ENCODER_DEBUG

                Serial.println(
                    "[Encoder] Debouncing press"
                );

#endif
            }

            break;


        // ====================================================
        // DEBOUNCING PRESS
        // ====================================================

        case BUTTON_DEBOUNCING_PRESS:

            if(
                !pressed
            )
            {
                buttonState =
                    BUTTON_RELEASED;


#ifdef ENCODER_DEBUG

                Serial.println(
                    "[Encoder] Debouncing press cancelled"
                );

#endif

                break;
            }


            if(
                now - buttonStateSince >=
                BUTTON_DEBOUNCE_TIME
            )
            {
                buttonState =
                    BUTTON_PRESSED;

                buttonStateSince =
                    now;


#ifdef ENCODER_DEBUG

                Serial.print(
                    "[Encoder] Button pressed: "
                );

                Serial.println(
                    buttonStateSince
                );

#endif
            }

            break;


        // ====================================================
        // PRESSED
        // ====================================================

        case BUTTON_PRESSED:

            if(
                !pressed
            )
            {
                buttonState =
                    BUTTON_DEBOUNCING_RELEASE_AFTER_PRESS;


                /*
                    Vanaf hier bewaren we de oorspronkelijke
                    indruktijd niet meer in buttonStateSince.

                    De indruktijd wordt daarom eerst berekend
                    voordat buttonStateSince wordt aangepast.
                */

                const unsigned long duration =
                    now - buttonStateSince;


#ifdef ENCODER_DEBUG

                Serial.print(
                    "[Encoder] Button released: "
                );

                Serial.print(
                    now
                );

                Serial.print(
                    " duration="
                );

                Serial.print(
                    duration
                );

                Serial.println(
                    " ms"
                );

#endif


                /*
                    Het uiteindelijke event wordt bepaald
                    door de totale indruktijd.
                */

                if(
                    duration >=
                    BUTTON_LONG_PRESS_TIME
                )
                {
                    EncoderEvent event;

                    event.type =
                        ENCODER_LONG_PRESS;

                    event.value =
                        duration;


                    xQueueSend(
                        eventQueue,
                        &event,
                        0
                    );


#ifdef ENCODER_DEBUG

                    Serial.println(
                        "[Encoder] EVENT: LONG_PRESS"
                    );

#endif
                }
                else
                {
                    EncoderEvent event;

                    event.type =
                        ENCODER_PRESS;

                    event.value =
                        duration;


                    xQueueSend(
                        eventQueue,
                        &event,
                        0
                    );


#ifdef ENCODER_DEBUG

                    Serial.println(
                        "[Encoder] EVENT: PRESS"
                    );

#endif
                }


                buttonStateSince =
                    now;

                break;
            }

            break;


        // ====================================================
        // DEBOUNCING RELEASE
        // ====================================================

        case BUTTON_DEBOUNCING_RELEASE_AFTER_PRESS:

            if(
                pressed
            )
            {
                buttonState =
                    BUTTON_DEBOUNCING_PRESS;

                buttonStateSince =
                    now;


#ifdef ENCODER_DEBUG

                Serial.println(
                    "[Encoder] Release bounce detected"
                );

#endif

                break;
            }


            if(
                now - buttonStateSince >=
                BUTTON_DEBOUNCE_TIME
            )
            {
                buttonState =
                    BUTTON_RELEASED;


#ifdef ENCODER_DEBUG

                Serial.println(
                    "[Encoder] Button release debounced"
                );

#endif
            }

            break;
    }
}


// ============================================================
// AVAILABLE
// ============================================================

bool Encoder::available()
{
    if(
        eventQueue ==
        nullptr
    )
    {
        return false;
    }


    return uxQueueMessagesWaiting(
        eventQueue
    ) > 0;
}


// ============================================================
// READ
// ============================================================

EncoderEvent Encoder::read()
{
    EncoderEvent event;


    event.type =
        ENCODER_NONE;

    event.value =
        0;


    if(
        eventQueue ==
        nullptr
    )
    {
        return event;
    }


    xQueueReceive(
        eventQueue,
        &event,
        0
    );


    return event;
}


// ============================================================
// INJECT PULSE
// ============================================================

void Encoder::injectPulse(
    int value
)
{
    if(
        eventQueue ==
        nullptr
    )
    {
        return;
    }


    EncoderEvent event;

    event.type =
        ENCODER_PULSE;

    event.value =
        value;


    xQueueSend(
        eventQueue,
        &event,
        0
    );
}
