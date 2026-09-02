#ifndef DISPLAY_H
#define DISPLAY_H

#include "Config.h"


class Display
{
public:

    bool begin();

    void clear();

    void setTitle(const char* text);
    void setStatus(const char* text);

    void setLine1(const char* text);
    void setLine2(const char* text);

    void update();


private:
    // TODO:
    // toekomstige OLED hardware/driver komt hier.
    // moet via begin binnenkomen en zoals de andere hardware dingen hoort hij uit main.cpp te komen (via controller)

    bool dirty = true;

    char title[21] = "";
    char status[21] = "";

    char line1[21] = "";
    char line2[21] = "";
};


#endif