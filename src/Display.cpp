#include "Display.h"

#include <string.h>


// ============================================================
// BEGIN
// ============================================================

bool Display::begin(
    OLED& oled
)
{
    this->oled =
        &oled;


    setView(
        VIEW_SIMPLE_TEXT_ONLY
    );


    dirty =
        true;


    return true;
}


// ============================================================
// CLEAR
// ============================================================

void Display::clear()
{
    if(
        oled == nullptr
    )
    {
        return;
    }


    oled->clear();
    oled->display();


    dirty =
        false;
}


// ============================================================
// SET VIEW
// ============================================================

void Display::setView(
    DisplayView view
)
{
    this->view =
        view;


    switch(
        view
    )
    {
        case VIEW_SIMPLE_TEXT_ONLY:

            viewHandler =
                &Display::updateSimpleTextOnly;

            break;


        case VIEW_ICON_RIGHT_TEXT:

            viewHandler =
                &Display::updateIconRightText;

            break;


        case VIEW_ICON_LEFT_TEXT:

            viewHandler =
                &Display::updateIconLeftText;

            break;
    }


    dirty =
        true;
}


// ============================================================
// SET ICON
// ============================================================

void Display::setIcon(
    const uint8_t* selected_icon,
    IconPosition position
)
{
    icon =
        selected_icon;

    iconPosition =
        position;


    dirty =
        true;
}


// ============================================================
// ICON RIGHT + TEXT VIEW
// ============================================================

void Display::iconRightTextView(
    const uint8_t* selected_icon,
    const char* line1,
    const char* line2
)
{
    iconTextView_(
        selected_icon,
        ICON_RIGHT,
        VIEW_ICON_RIGHT_TEXT,
        line1,
        line2
    );
}


// ============================================================
// ICON LEFT + TEXT VIEW
// ============================================================

void Display::iconLeftTextView(
    const uint8_t* selected_icon,
    const char* line1,
    const char* line2
)
{
    iconTextView_(
        selected_icon,
        ICON_LEFT,
        VIEW_ICON_LEFT_TEXT,
        line1,
        line2
    );
}


// ============================================================
// ICON + TEXT VIEW
// ============================================================

void Display::iconTextView_(
    const uint8_t* selected_icon,
    IconPosition position,
    DisplayView view,
    const char* line1,
    const char* line2
)
{
    setView(
        view
    );


    setIcon(
        selected_icon,
        position
    );


    setLine1(
        line1
    );

    setLine2(
        line2
    );
}


// ============================================================
// SET TITLE
// ============================================================

void Display::setTitle(
    const char* text
)
{
    strncpy(
        title,
        text ? text : "",
        sizeof(title) - 1
    );

    title[sizeof(title) - 1] =
        '\0';


    dirty =
        true;
}


// ============================================================
// SET STATUS
// ============================================================

void Display::setStatus(
    const char* text
)
{
    strncpy(
        status,
        text ? text : "",
        sizeof(status) - 1
    );

    status[sizeof(status) - 1] =
        '\0';


    dirty =
        true;
}


// ============================================================
// SET LINE 1
// ============================================================

void Display::setLine1(
    const char* text
)
{
    strncpy(
        line1,
        text ? text : "",
        sizeof(line1) - 1
    );

    line1[sizeof(line1) - 1] =
        '\0';


    dirty =
        true;
}


// ============================================================
// SET LINE 2
// ============================================================

void Display::setLine2(
    const char* text
)
{
    strncpy(
        line2,
        text ? text : "",
        sizeof(line2) - 1
    );

    line2[sizeof(line2) - 1] =
        '\0';


    dirty =
        true;
}


// ============================================================
// UPDATE
// ============================================================

void Display::update()
{
    if(
        oled == nullptr ||
        !dirty
    )
    {
        return;
    }


    oled->clear();


    oled->setTextSize(
        1
    );

    oled->setTextColor(
        true
    );

    oled->setTextWrap(
        false
    );


    if(
        viewHandler != nullptr
    )
    {
        (this->*viewHandler)();
    }


    oled->display();


    dirty =
        false;
}


// ============================================================
// VIEW: SIMPLE TEXT ONLY
// ============================================================

void Display::updateSimpleTextOnly()
{
    oled->setCursor(
        0,
        0
    );

    oled->print(
        title
    );


    oled->setCursor(
        0,
        8
    );

    oled->print(
        status
    );


    oled->setCursor(
        0,
        16
    );

    oled->print(
        line1
    );


    oled->setCursor(
        0,
        24
    );

    oled->print(
        line2
    );
}


// ============================================================
// VIEW: ICON RIGHT + TEXT
// ============================================================

void Display::updateIconRightText()
{
    const int ICON_X =
        100;

    const int ICON_Y =
        2;

    const int TEXT_X =
        0;


    oled->setCursor(
        TEXT_X,
        0
    );

    oled->print(
        title
    );


    oled->setCursor(
        TEXT_X,
        8
    );

    oled->print(
        status
    );


    oled->setCursor(
        TEXT_X,
        16
    );

    oled->print(
        line1
    );


    oled->setCursor(
        TEXT_X,
        24
    );

    oled->print(
        line2
    );


    if(
        icon != nullptr
    )
    {
        oled->drawXBitmap(
            ICON_X,
            ICON_Y,
            icon,
            28,
            28,
            true
        );
    }
}


// ============================================================
// VIEW: ICON LEFT + TEXT
// ============================================================

void Display::updateIconLeftText()
{
    const int ICON_X =
        0;

    const int ICON_Y =
        2;

    const int TEXT_X =
        44;


    oled->setCursor(
        TEXT_X,
        0
    );

    oled->print(
        title
    );


    oled->setCursor(
        TEXT_X,
        8
    );

    oled->print(
        status
    );


    oled->setCursor(
        TEXT_X,
        16
    );

    oled->print(
        line1
    );


    oled->setCursor(
        TEXT_X,
        24
    );

    oled->print(
        line2
    );


    if(
        icon != nullptr
    )
    {
        oled->drawXBitmap(
            ICON_X,
            ICON_Y,
            icon,
            28,
            28,
            true
        );
    }
}