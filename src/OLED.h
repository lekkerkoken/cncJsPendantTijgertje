#ifndef OLED_H
#define OLED_H

#include <Arduino.h>


class OLED
{
public:

    bool begin();

    void clear();

    void setCursor(
        int16_t x,
        int16_t y
    );

    void setTextSize(
        uint8_t size
    );

    void setTextColor(
        bool white
    );

    void setTextWrap(
        bool wrap
    );

    void print(
        const char* text
    );

    void display();
};

#endif