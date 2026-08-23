#include "ControllerPreference.h"


// ============================================================
// BEGIN
// ============================================================

void ControllerPreference::begin()
{
    preferences.begin(
        "cncjs",
        false
    );


    preferredPort =
        preferences.getString(
            "port",
            ""
        );


    preferredControllerType =
        preferences.getString(
            "controller",
            ""
        );


    preferredBaudrate =
        preferences.getInt(
            "baudrate",
            0
        );


    preferenceAvailable =
        preferredPort.length() > 0 &&
        preferredControllerType.length() > 0 &&
        preferredBaudrate > 0;


    Serial.println();
    Serial.println(
        "[Preference] Controller"
    );


    if (preferenceAvailable)
    {
        Serial.print(
            "  Port: "
        );

        Serial.println(
            preferredPort
        );


        Serial.print(
            "  Controller: "
        );

        Serial.println(
            preferredControllerType
        );


        Serial.print(
            "  Baudrate: "
        );

        Serial.println(
            preferredBaudrate
        );
    }
    else
    {
        Serial.println(
            "  No stored controller"
        );
    }
}


// ============================================================
// HAS PREFERENCE
// ============================================================

bool ControllerPreference::hasPreference() const
{
    return preferenceAvailable;
}


// ============================================================
// PORT
// ============================================================

const char* ControllerPreference::port() const
{
    return preferredPort.c_str();
}


// ============================================================
// CONTROLLER TYPE
// ============================================================

const char* ControllerPreference::controllerType() const
{
    return preferredControllerType.c_str();
}


// ============================================================
// BAUDRATE
// ============================================================

int ControllerPreference::baudrate() const
{
    return preferredBaudrate;
}


// ============================================================
// SAVE
// ============================================================

void ControllerPreference::save(
    const char* port,
    const char* controllerType,
    int baudrate
)
{
    if (
        port == nullptr ||
        controllerType == nullptr ||
        baudrate <= 0
    )
    {
        return;
    }


    preferences.putString(
        "port",
        port
    );


    preferences.putString(
        "controller",
        controllerType
    );


    preferences.putInt(
        "baudrate",
        baudrate
    );


    preferredPort =
        port;


    preferredControllerType =
        controllerType;


    preferredBaudrate =
        baudrate;


    preferenceAvailable =
        true;


    Serial.println();

    Serial.println(
        "[Preference] Controller saved"
    );


    Serial.print(
        "  Port: "
    );

    Serial.println(
        preferredPort
    );


    Serial.print(
        "  Controller: "
    );

    Serial.println(
        preferredControllerType
    );


    Serial.print(
        "  Baudrate: "
    );

    Serial.println(
        preferredBaudrate
    );
}


// ============================================================
// CLEAR
// ============================================================

void ControllerPreference::clear()
{
    preferences.clear();


    preferredPort = "";
    preferredControllerType = "";
    preferredBaudrate = 0;

    preferenceAvailable = false;


    Serial.println(
        "[Preference] Controller preference cleared"
    );
}