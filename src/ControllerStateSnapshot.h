#ifndef CONTROLLER_STATE_SNAPSHOT_H
#define CONTROLLER_STATE_SNAPSHOT_H

#include <Arduino.h>
#include <ArduinoJson.h>


struct ControllerStateSnapshot
{
    String controllerType;

    JsonDocument state;


    void clear()
    {
        controllerType =
            "";

        state.clear();
    }
};


#endif