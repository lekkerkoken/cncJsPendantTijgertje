#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>
#include "Config.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>


// ============================================================
// ENCODER EVENT
// ============================================================

enum EncoderEventType
{
    ENCODER_NONE,
    ENCODER_PULSE,
    ENCODER_PRESS,
    ENCODER_LONG_PRESS
};


struct EncoderEvent
{
    EncoderEventType type = ENCODER_NONE;
    int value = 0;
};


// ============================================================
// ENCODER
// ============================================================

class Encoder
{
public:

    void begin(
        int pinA,
        int pinB,
        int buttonPin
    );

    // Compatibility
    void update();

    bool available();

    EncoderEvent read();

    // Test / event injection
    void injectPulse(
        int value
    );


private:

    int pinA = -1;
    int pinB = -1;
    int buttonPin = -1;


    // ========================================================
    // FREERTOS
    // ========================================================

    QueueHandle_t eventQueue = nullptr;

    TaskHandle_t taskHandle = nullptr;


    static constexpr int EVENT_QUEUE_LENGTH = 32;

    static constexpr uint32_t TASK_STACK_SIZE = 2048;

    static constexpr UBaseType_t TASK_PRIORITY = 3;

    static constexpr uint32_t TASK_DELAY_MS = 1;


    static void taskEntry(
        void* parameter
    );

    void task();


    // ========================================================
    // ROTARY ENCODER
    // ========================================================

    volatile int encoderAccumulator = 0;

    uint8_t lastEncoderState = 1;


    /*
        Gedeelde critical section voor:

        - encoderAccumulator
        - buttonChanged

        Zowel encoder- als button-ISR kunnen deze gebruiken.
    */

    portMUX_TYPE encoderMux =
        portMUX_INITIALIZER_UNLOCKED;


    static void ARDUINO_ISR_ATTR encoderISR(
        void* parameter
    );

    void handleEncoderTransition();


    // ========================================================
    // ENCODER BUTTON
    // ========================================================

    /*
        De ISR zet deze flag wanneer de fysieke GPIO
        van de button verandert.

        De flag wordt NIET gebruikt om direct een event
        te genereren.

        De Encoder task leest de flag en laat vervolgens
        de normale button state machine het werk doen.
    */

    volatile bool buttonChanged = false;


    static void ARDUINO_ISR_ATTR buttonISR(
        void* parameter
    );


    static constexpr unsigned long BUTTON_DEBOUNCE_TIME = 10;

    /*
        Tijdelijk hoog ingesteld voor debuggen.

        Later terugzetten naar bijvoorbeeld 800 ms.
    */

    static constexpr unsigned long BUTTON_LONG_PRESS_TIME = 1200;


    enum ButtonState
    {
        BUTTON_RELEASED,

        BUTTON_DEBOUNCING_PRESS,

        BUTTON_PRESSED,

        BUTTON_LONG_PRESS,

        BUTTON_DEBOUNCING_RELEASE_AFTER_PRESS,

        BUTTON_DEBOUNCING_RELEASE_AFTER_LONG_PRESS
    };


    ButtonState buttonState =
        BUTTON_RELEASED;


    /*
        Tijdstip waarop de huidige button-state
        is begonnen.
    */

    unsigned long buttonStateSince =
        0;


    void updateButton();
};

#endif