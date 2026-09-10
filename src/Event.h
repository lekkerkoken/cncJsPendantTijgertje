#ifndef EVENT_H
#define EVENT_H


enum EventType
{
    EVENT_NONE = 0,


    // Matrix toetsen

    EVENT_KEY_1,
    EVENT_KEY_2,
    EVENT_KEY_3,
    EVENT_KEY_4,
    EVENT_KEY_5,
    EVENT_KEY_6,
    EVENT_KEY_7,
    EVENT_KEY_8,
    EVENT_KEY_9,


    // Encoder

    EVENT_ENCODER_PULSE,
    EVENT_ENCODER_PRESS,
    EVENT_ENCODER_LONG_PRESS,


    // CNCjs state events

    EVENT_CHANGED_TO_READY
};


struct Event
{
    EventType type = EVENT_NONE;

    int value = 0;
};


#endif