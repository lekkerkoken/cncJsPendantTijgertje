#ifndef CONTROLLER_STATE_SNAPSHOT_H
#define CONTROLLER_STATE_SNAPSHOT_H

#include <Arduino.h>
#include <ArduinoJson.h>


struct ControllerStateSnapshot
{
    bool valid = false;

    String controllerType;

    JsonDocument state;


    void invalidate()
    {
        valid =
            false;

        controllerType =
            "";

        state.clear();
    }
};

#endif