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
// SHOW JOG ICON
// ============================================================

void Display::showJogIcon(
    Axis axis
)
{
    switch(
        axis
    )
    {
        case AXIS_X:

            jogIcon =
                JOG_ICON_X;

            break;


        case AXIS_Y:

            jogIcon =
                JOG_ICON_Y;

            break;


        case AXIS_Z:

            jogIcon =
                JOG_ICON_Z;

            break;


        default:

            jogIcon =
                nullptr;

            break;
    }


    dirty =
        true;
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


    // --------------------------------------------------------
    // 128 x 32 OLED
    // Vier regels van 8 pixels
    // --------------------------------------------------------

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

    if(
        jogIcon != nullptr
    )
    {
        oled->drawXBitmap(
            (128 - JOG_ICON_WIDTH),
            (32 - JOG_ICON_HEIGHT) / 2,
            jogIcon,
            JOG_ICON_WIDTH,
            JOG_ICON_HEIGHT,
            true
        );
    }
    


    oled->display();


    dirty =
        false;
}