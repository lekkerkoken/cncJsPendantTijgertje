#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>


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

    // --------------------------------------------------------
    // Compatibility
    // --------------------------------------------------------

    void update();

    bool available();
//nu kan input ook zijn llllll
//of lllr
//of rrrrrr

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


    // --------------------------------------------------------
    // FreeRTOS
    // --------------------------------------------------------

    QueueHandle_t eventQueue = nullptr;

    TaskHandle_t taskHandle = nullptr;


    static constexpr int EVENT_QUEUE_LENGTH = 16;

    static constexpr uint32_t TASK_STACK_SIZE = 2048;

    static constexpr UBaseType_t TASK_PRIORITY = 3;

    static constexpr uint32_t TASK_DELAY_MS = 1;


    static void taskEntry(
        void* parameter
    );

    void task();


    // --------------------------------------------------------
    // Rotary encoder
    // --------------------------------------------------------

    volatile int encoderAccumulator = 0;

    uint8_t lastEncoderState = 0;


    portMUX_TYPE encoderMux =
        portMUX_INITIALIZER_UNLOCKED;


    static void ARDUINO_ISR_ATTR encoderISR(
        void* parameter
    );

    void handleEncoderTransition();


    // --------------------------------------------------------
    // Encoder button
    // --------------------------------------------------------

    static constexpr unsigned long BUTTON_DEBOUNCE_TIME = 10;

    bool stableButtonState = HIGH;

    bool candidateButtonState = HIGH;

    unsigned long candidateButtonSince = 0;


    void updateButton();
};


#endif