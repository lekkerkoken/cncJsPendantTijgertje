#ifndef OLED_H
#define OLED_H

#include <Arduino.h>


class OLED
{
public:

    bool begin();

    void clear();

    void drawXBitmap(
        int16_t x,
        int16_t y,
        const uint8_t* bitmap,
        int16_t width,
        int16_t height,
        bool color
    );

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