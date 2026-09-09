#ifndef SENDER_STATUS_SNAPSHOT_H
#define SENDER_STATUS_SNAPSHOT_H

#include <Arduino.h>
#include <ArduinoJson.h>


struct SenderStatusSnapshot
{
    bool valid = false;

    JsonDocument status;


    void invalidate()
    {
        valid =
            false;

        status.clear();
    }
};

#endif