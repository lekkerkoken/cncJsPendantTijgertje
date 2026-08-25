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


    // --------------------------------------------------------
    // Event queue
    // --------------------------------------------------------

    eventQueue =
        xQueueCreate(
            EVENT_QUEUE_LENGTH,
            sizeof(EncoderEvent)
        );


    if(eventQueue == nullptr)
    {
        Serial.println(
            "[Encoder] ERROR: Could not create event queue"
        );

        return;
    }


    // --------------------------------------------------------
    // Encoder initial state
    // --------------------------------------------------------

    lastEncoderState =
        (digitalRead(this->pinA) << 1) |
         digitalRead(this->pinB);


    portENTER_CRITICAL(&encoderMux);

    encoderAccumulator =
        0;

    portEXIT_CRITICAL(&encoderMux);


    // --------------------------------------------------------
    // Button initial state
    // --------------------------------------------------------

    stableButtonState =
        digitalRead(this->buttonPin);

    candidateButtonState =
        stableButtonState;

    candidateButtonSince =
        millis();


    // --------------------------------------------------------
    // Encoder interrupts
    // --------------------------------------------------------

    attachInterruptArg(
        this->pinA,
        Encoder::encoderISR,
        this,
        CHANGE
    );

    attachInterruptArg(
        this->pinB,
        Encoder::encoderISR,
        this,
        CHANGE
    );


    // --------------------------------------------------------
    // Encoder task
    // --------------------------------------------------------

    BaseType_t result =
        xTaskCreate(
            Encoder::taskEntry,
            "Encoder",
            TASK_STACK_SIZE,
            this,
            TASK_PRIORITY,
            &taskHandle
        );


    if(result != pdPASS)
    {
        taskHandle =
            nullptr;

        Serial.println(
            "[Encoder] ERROR: Could not create task"
        );
    }
}



// ============================================================
// UPDATE
// ============================================================

void Encoder::update()
{
    /*
        Encoder wordt nu zelfstandig verwerkt door
        de FreeRTOS task en GPIO interrupts.

        Deze functie blijft voorlopig bestaan als
        compatibility interface.
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


    if(encoder == nullptr)
        return;


    encoder->handleEncoderTransition();
}



// ============================================================
// HANDLE ENCODER TRANSITION
// ============================================================

void Encoder::handleEncoderTransition()
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


    uint8_t currentState =
        (digitalRead(pinA) << 1) |
         digitalRead(pinB);


    if(currentState == lastEncoderState)
        return;


    uint8_t transition =
        (lastEncoderState << 2) |
        currentState;


    int8_t delta =
        transitionTable[transition];


    lastEncoderState =
        currentState;


    if(delta == 0)
        return;


    portENTER_CRITICAL_ISR(&encoderMux);

    encoderAccumulator +=
        delta;

    portEXIT_CRITICAL_ISR(&encoderMux);
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


    if(encoder != nullptr)
    {
        encoder->task();
    }


    vTaskDelete(nullptr);
}



// ============================================================
// TASK
// ============================================================

void Encoder::task()
{
    for(;;)
    {
        // ====================================================
        // ROTARY ENCODER
        // ====================================================

        int delta =
            0;


        portENTER_CRITICAL(&encoderMux);

        /*
            Eén volledige quadrature cyclus bestaat uit
            vier geldige transities.

            De ISR verzamelt de transities.
            De task vertaalt volledige cycli naar
            fysieke encoderpulsen.
        */

        while(encoderAccumulator >= 2)
        {
            delta++;

            encoderAccumulator -=
                2;
        }


        while(encoderAccumulator <= -2)
        {
            delta--;

            encoderAccumulator +=
                2;
        }


        portEXIT_CRITICAL(&encoderMux);


        /*
            Meerdere fysieke stappen die sinds de vorige
            task-run zijn gemaakt worden hier samengevoegd.

            Dit is nog steeds een EncoderEvent-stream:
            de echte applicatie-coalescing gebeurt later
            in InputManager.
        */

        if(delta != 0)
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
        }


        // ====================================================
        // BUTTON
        // ====================================================

        updateButton();


        vTaskDelay(
            pdMS_TO_TICKS(TASK_DELAY_MS)
        );
    }
}



// ============================================================
// UPDATE BUTTON
// ============================================================

void Encoder::updateButton()
{
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
        EncoderEvent event;

        event.type =
            ENCODER_PRESS;

        event.value =
            0;


        xQueueSend(
            eventQueue,
            &event,
            0
        );
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


    if(eventQueue == nullptr)
        return;


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



// ============================================================
// AVAILABLE
// ============================================================

bool Encoder::available()
{
    if(eventQueue == nullptr)
        return false;


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


    if(eventQueue == nullptr)
        return event;


    xQueueReceive(
        eventQueue,
        &event,
        0
    );


    return event;
}