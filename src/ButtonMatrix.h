#ifndef BUTTON_MATRIX_H
#define BUTTON_MATRIX_H

#include <Arduino.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>


class ButtonMatrix
{
public:

    void begin();

    // --------------------------------------------------------
    // Compatibility
    // --------------------------------------------------------

    void update();

    bool available();

    int read();

    // Huidige fysieke toestand van de matrix
    int currentKey() const;


private:

    // --------------------------------------------------------
    // FreeRTOS
    // --------------------------------------------------------

    QueueHandle_t eventQueue = nullptr;

    TaskHandle_t taskHandle = nullptr;


    static constexpr int EVENT_QUEUE_LENGTH = 8;

    static constexpr uint32_t TASK_STACK_SIZE = 2048;

    static constexpr UBaseType_t TASK_PRIORITY = 2;

    static constexpr uint32_t TASK_DELAY_MS = 1;


    static constexpr unsigned long DEBOUNCE_TIME = 10;


    static void taskEntry(
        void* parameter
    );

    void task();


    // --------------------------------------------------------
    // Matrix state
    // --------------------------------------------------------

    int currentKeyState = -1;

    int stableKey = -1;

    int candidateKey = -1;

    unsigned long candidateSince = 0;


    int scan();


    // --------------------------------------------------------
    // Legacy compatibility
    // --------------------------------------------------------

    int lastKey = -1;
};


#endif