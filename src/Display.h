#ifndef DISPLAY_H
#define DISPLAY_H

#include "OLED.h"
#include "Icons.h"
#include "PendantState.h"

enum IconPosition
{
    ICON_LEFT,
    ICON_RIGHT
};

class Display
{
public:

    bool begin(
        OLED& oled
    );

    void clear();

    void setIcon(
        const uint8_t* selected_icon,
        IconPosition position
    );

    void setTitle(
        const char* text
    );

    void setStatus(
        const char* text
    );

    void setLine1(
        const char* text
    );

    void setLine2(
        const char* text
    );

    void update();


private:

    OLED* oled =
        nullptr;


    bool dirty =
        true;


    char title[21] = "";
    char status[21] = "";
    char line1[21] = "";
    char line2[21] = "";

    const uint8_t* icon =
        nullptr;

    IconPosition iconPosition =
        ICON_RIGHT;
};

#endif