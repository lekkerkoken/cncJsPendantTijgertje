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


enum DisplayView
{
    VIEW_SIMPLE_TEXT_ONLY,
    VIEW_ICON_RIGHT_TEXT,
    VIEW_ICON_LEFT_TEXT
};


class Display
{
public:

    bool begin(
        OLED& oled
    );

    void clear();


    void setView(
        DisplayView view
    );


    void setIcon(
        const uint8_t* selected_icon,
        IconPosition position
    );


    void iconRightTextView(
        const uint8_t* selected_icon,
        const char* line1,
        const char* line2
    );

    void iconLeftTextView(
        const uint8_t* selected_icon,
        const char* line1,
        const char* line2
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


    DisplayView view =
        VIEW_SIMPLE_TEXT_ONLY;


    void (Display::*viewHandler)() =
        nullptr;


    char title[21] = "";
    char status[21] = "";
    char line1[21] = "";
    char line2[21] = "";


    const uint8_t* icon =
        nullptr;

    IconPosition iconPosition =
        ICON_RIGHT;


    void iconTextView_(
        const uint8_t* selected_icon,
        IconPosition position,
        DisplayView view,
        const char* line1,
        const char* line2
    );


    void updateSimpleTextOnly();

    void updateIconRightText();

    void updateIconLeftText();
};

#endif