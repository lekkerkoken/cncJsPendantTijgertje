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


    // --------------------------------------------------------
    // Output event queue
    // --------------------------------------------------------

    eventQueue =
        xQueueCreate(
            EVENT_QUEUE_LENGTH,
            sizeof(Event)
        );


    if(eventQueue == nullptr)
    {
        Serial.println(
            "[InputManager] ERROR: Could not create event queue"
        );

        return;
    }


    // --------------------------------------------------------
    // Encoder coalescing state
    // --------------------------------------------------------

    encoderCoalescedDelta =
        0;

    encoderDirection =
        0;


    // --------------------------------------------------------
    // InputManager task
    // --------------------------------------------------------

    BaseType_t result =
        xTaskCreate(
            InputManager::taskEntry,
            "InputManager",
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
            "[InputManager] ERROR: Could not create task"
        );
    }
}



// ============================================================
// TASK ENTRY
// ============================================================

void InputManager::taskEntry(
    void* parameter
)
{
    InputManager* inputManager =
        static_cast<InputManager*>(parameter);


    if(inputManager != nullptr)
    {
        inputManager->task();
    }


    vTaskDelete(nullptr);
}



// ============================================================
// TASK
// ============================================================

void InputManager::task()
{
    for(;;)
    {
        bool didWork =
            false;


        // ====================================================
        // ENCODER
        // ====================================================

        if(encoder != nullptr)
        {
            while(
                encoder->available()
            )
            {
                EncoderEvent encoderEventData =
                    encoder->read();


                switch(
                    encoderEventData.type
                )
                {
                    // ========================================
                    // ROTARY PULSE
                    // ========================================

                    case ENCODER_PULSE:
                    {
                        int pulse =
                            encoderEventData.value;


                        if(pulse == 0)
                            break;


                        int direction =
                            pulse > 0
                                ? +1
                                : -1;


                        /*
                            Een richtingsverandering is een
                            expliciete grens tussen twee
                            bedieningsintenties.

                            De bestaande batch wordt daarom
                            eerst gepubliceerd.
                        */

                        if(
                            encoderDirection != 0 &&
                            direction != encoderDirection
                        )
                        {
                            flushEncoderDelta();
                        }


                        /*
                            Start een nieuwe batch wanneer
                            nodig.
                        */

                        encoderDirection =
                            direction;


                        /*
                            Meerdere pulsen in dezelfde
                            richting worden samengevoegd.
                        */

                        encoderCoalescedDelta +=
                            pulse;


                        didWork =
                            true;

                        break;
                    }


                    // ========================================
                    // ENCODER PRESS
                    // ========================================

                    case ENCODER_PRESS:
                    {
                        /*
                            Een press is een afzonderlijk
                            event en wordt nooit met een
                            encoder-delta gecombineerd.
                        */

                        flushEncoderDelta();


                        Event event =
                            encoderEvent(
                                encoderEventData
                            );


                        if(event.type != EVENT_NONE)
                        {
                            xQueueSend(
                                eventQueue,
                                &event,
                                0
                            );
                        }


                        didWork =
                            true;

                        break;
                    }


                    case ENCODER_NONE:

                        break;
                }
            }


            /*
                Publiceer de laatst verzamelde encoderbatch.

                Een volgende task-run kan daarna opnieuw
                dezelfde richting verzamelen.

                Een richtingsverandering wordt hierboven
                al onmiddellijk als batch-grens behandeld.
            */

            flushEncoderDelta();
        }


        // ====================================================
        // BUTTON MATRIX
        // ====================================================

        if(matrix != nullptr)
        {
            while(
                matrix->available()
            )
            {
                int key =
                    matrix->read();


                Event event =
                    buttonEvent(key);


                if(event.type != EVENT_NONE)
                {
                    xQueueSend(
                        eventQueue,
                        &event,
                        0
                    );


                    didWork =
                        true;


                    // ------------------------------------------------
                    // DEBUG
                    // ------------------------------------------------

                    Serial.print(
                        "[InputManager] Key event: "
                    );

                    Serial.print(
                        key
                    );

                    Serial.print(
                        " -> event type "
                    );

                    Serial.println(
                        event.type
                    );
                }
            }
        }


        /*
            Bij input blijven we direct opnieuw kijken.
            Zonder input geven we de CPU weer vrij.
        */

        if(!didWork)
        {
            vTaskDelay(
                pdMS_TO_TICKS(TASK_DELAY_MS)
            );
        }
        else
        {
            taskYIELD();
        }
    }
}



// ============================================================
// FLUSH ENCODER DELTA
// ============================================================

void InputManager::flushEncoderDelta()
{
    if(encoderCoalescedDelta == 0)
    {
        encoderDirection =
            0;

        return;
    }


    Event event;

    event.type =
        EVENT_ENCODER_PULSE;

    event.value =
        encoderCoalescedDelta;


    xQueueSend(
        eventQueue,
        &event,
        0
    );


    /*
        Batch is gepubliceerd.
    */

    encoderCoalescedDelta =
        0;

    encoderDirection =
        0;
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
    if(eventQueue == nullptr)
        return false;


    return uxQueueMessagesWaiting(
        eventQueue
    ) > 0;
}



// ============================================================
// READ
// ============================================================

Event InputManager::read()
{
    Event event;

    event.type =
        EVENT_NONE;

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