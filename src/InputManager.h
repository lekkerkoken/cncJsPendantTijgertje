#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H


#include <Arduino.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>


#include "Event.h"
#include "ButtonMatrix.h"
#include "Encoder.h"


class InputManager
{
public:

    void begin(
        ButtonMatrix& matrix,
        Encoder& encoder
    );


    bool available();

    Event read();


private:

    ButtonMatrix* matrix = nullptr;

    Encoder* encoder = nullptr;


    // --------------------------------------------------------
    // FreeRTOS
    // --------------------------------------------------------

    QueueHandle_t eventQueue = nullptr;

    TaskHandle_t taskHandle = nullptr;


    static constexpr int EVENT_QUEUE_LENGTH = 16;

    static constexpr uint32_t TASK_STACK_SIZE = 2048;

    static constexpr UBaseType_t TASK_PRIORITY = 2;

    static constexpr uint32_t TASK_DELAY_MS = 1;


    static void taskEntry(
        void* parameter
    );

    void task();


    // --------------------------------------------------------
    // Encoder coalescing
    // --------------------------------------------------------

    int encoderCoalescedDelta = 0;

    int encoderDirection = 0;


    void flushEncoderDelta();


    // --------------------------------------------------------
    // Event creation
    // --------------------------------------------------------

    Event buttonEvent(
        int key
    );

    Event encoderEvent(
        EncoderEvent event
    );
};


#endif