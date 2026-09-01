#ifndef CONTROLLER_SETTINGS_SNAPSHOT_H
#define CONTROLLER_SETTINGS_SNAPSHOT_H

#include <Arduino.h>
#include <ArduinoJson.h>


struct ControllerSettingsSnapshot
{
    String controllerType;

    JsonDocument settings;

    void clear()
    {
        controllerType =
            "";

        settings.clear();
    }
};


#endif