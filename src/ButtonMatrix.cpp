#include "ButtonMatrix.h"
#include "Config.h"

#include <Arduino.h>


const int rows[] =
{
    D8,
    D9,
    D10
};


const int cols[] =
{
    D2,
    D1,
    D0
};


const int ROWS = MATRIX_ROWS;
const int COLS = MATRIX_COLS;



// ============================================================
// BEGIN
// ============================================================

void ButtonMatrix::begin()
{
    for(int r = 0; r < ROWS; r++)
    {
        pinMode(
            MATRIX_ROW_PINS[r],
            OUTPUT
        );

        digitalWrite(
            MATRIX_ROW_PINS[r],
            HIGH
        );
    }


    for(int c = 0; c < COLS; c++)
    {
        pinMode(
            MATRIX_COL_PINS[c],
            INPUT_PULLUP
        );
    }


    currentKeyState =
        -1;

    stableKey =
        -1;

    candidateKey =
        -1;

    candidateSince =
        millis();

    lastKey =
        -1;


    // --------------------------------------------------------
    // Event queue
    // --------------------------------------------------------

    eventQueue =
        xQueueCreate(
            EVENT_QUEUE_LENGTH,
            sizeof(int)
        );


    if(eventQueue == nullptr)
    {
        Serial.println(
            "[ButtonMatrix] ERROR: Could not create event queue"
        );

        return;
    }


    // --------------------------------------------------------
    // Matrix task
    // --------------------------------------------------------

    BaseType_t result =
        xTaskCreate(
            ButtonMatrix::taskEntry,
            "ButtonMatrix",
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
            "[ButtonMatrix] ERROR: Could not create task"
        );
    }
}



// ============================================================
// UPDATE
// ============================================================

void ButtonMatrix::update()
{
    /*
        Matrix wordt nu zelfstandig gescand door
        de FreeRTOS task.

        Deze functie blijft voorlopig bestaan als
        compatibility interface.
    */
}



// ============================================================
// SCAN
// ============================================================

int ButtonMatrix::scan()
{
    int detectedKey =
        -1;


    for(int r = 0; r < ROWS; r++)
    {
        digitalWrite(
            rows[r],
            LOW
        );


        for(int c = 0; c < COLS; c++)
        {
            if(
                digitalRead(cols[c]) == LOW
            )
            {
                detectedKey =
                    r * COLS + c + 1;

                break;
            }
        }


        digitalWrite(
            rows[r],
            HIGH
        );


        if(detectedKey != -1)
        {
            break;
        }
    }


    return detectedKey;
}



// ============================================================
// TASK ENTRY
// ============================================================

void ButtonMatrix::taskEntry(
    void* parameter
)
{
    ButtonMatrix* matrix =
        static_cast<ButtonMatrix*>(parameter);


    if(matrix != nullptr)
    {
        matrix->task();
    }


    vTaskDelete(nullptr);
}



// ============================================================
// TASK
// ============================================================

void ButtonMatrix::task()
{
    for(;;)
    {
        int detectedKey =
            scan();


        currentKeyState =
            detectedKey;


        /*
            Fysieke toestand is veranderd.
            Start opnieuw met debouncen.
        */

        if(detectedKey != candidateKey)
        {
            candidateKey =
                detectedKey;

            candidateSince =
                millis();

            vTaskDelay(
                pdMS_TO_TICKS(TASK_DELAY_MS)
            );

            continue;
        }


        /*
            Toestand moet lang genoeg stabiel zijn.
        */

        if(
            millis() - candidateSince <
            DEBOUNCE_TIME
        )
        {
            vTaskDelay(
                pdMS_TO_TICKS(TASK_DELAY_MS)
            );

            continue;
        }


        /*
            Toestand is stabiel, maar niet veranderd
            ten opzichte van de laatst geaccepteerde
            toestand.
        */

        if(candidateKey == stableKey)
        {
            vTaskDelay(
                pdMS_TO_TICKS(TASK_DELAY_MS)
            );

            continue;
        }


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
            int key =
                stableKey;


            xQueueSend(
                eventQueue,
                &key,
                0
            );


            lastKey =
                key;
        }


        vTaskDelay(
            pdMS_TO_TICKS(TASK_DELAY_MS)
        );
    }
}



// ============================================================
// AVAILABLE
// ============================================================

bool ButtonMatrix::available()
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

int ButtonMatrix::read()
{
    int key =
        -1;


    if(eventQueue == nullptr)
        return key;


    xQueueReceive(
        eventQueue,
        &key,
        0
    );


    lastKey =
        -1;


    return key;
}



// ============================================================
// CURRENT KEY
// ============================================================

int ButtonMatrix::currentKey() const
{
    return currentKeyState;
}