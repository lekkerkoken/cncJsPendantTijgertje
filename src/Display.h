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

    bool dirty = true;

    char title[21] = "";
    char status[21] = "";

    char line1[21] = "";
    char line2[21] = "";
};


#endif