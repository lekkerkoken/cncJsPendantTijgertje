#include "Display.h"

#include <Arduino.h>
#include <string.h>


#ifdef DISPLAY_DEBUG


bool Display::begin()
{
    Serial.println("Display initialized: DEBUG");

    return true;
}


void Display::clear()
{
    Serial.println("----------------");
}


#else

// OLED komt hier later


#endif



void Display::setTitle(const char* text)
{
    strncpy(title, text, sizeof(title) - 1);
    title[sizeof(title)-1] = '\0';

    dirty = true;
}



void Display::setStatus(const char* text)
{
    strncpy(status, text, sizeof(status) - 1);
    status[sizeof(status)-1] = '\0';

    dirty = true;
}



void Display::setLine1(const char* text)
{
    strncpy(line1, text, sizeof(line1) - 1);
    line1[sizeof(line1)-1] = '\0';

    dirty = true;
}



void Display::setLine2(const char* text)
{
    strncpy(line2, text, sizeof(line2) - 1);
    line2[sizeof(line2)-1] = '\0';

    dirty = true;
}



void Display::update()
{
    if(!dirty)
        return;


#ifdef DISPLAY_DEBUG

    Serial.println();
    Serial.println("----------------");
    Serial.println(title);
    Serial.println(status);
    Serial.println(line1);
    Serial.println(line2);
    Serial.println("----------------");


#else

    // OLED rendering komt later


#endif


    dirty = false;
}