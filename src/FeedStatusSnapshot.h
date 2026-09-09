#ifndef FEED_STATUS_SNAPSHOT_H
#define FEED_STATUS_SNAPSHOT_H

#include <Arduino.h>
#include <ArduinoJson.h>


struct FeedStatusSnapshot
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