#ifndef CONTROLLER_SETTINGS_SNAPSHOT_H
#define CONTROLLER_SETTINGS_SNAPSHOT_H

#include <Arduino.h>
#include <ArduinoJson.h>


struct ControllerSettingsSnapshot
{
    bool valid = false;

    String controllerType;

    JsonDocument payload;


    void invalidate()
    {
        valid =
            false;

        controllerType =
            "";

        payload.clear();
    }
};


#endif