#ifndef CONTROLLER_PREFERENCE_H
#define CONTROLLER_PREFERENCE_H

#include <Arduino.h>
#include <Preferences.h>


class ControllerPreference
{
public:

    void begin();


    // --------------------------------------------------------
    // Preference beschikbaar?
    // --------------------------------------------------------

    bool hasPreference() const;


    // --------------------------------------------------------
    // Opgeslagen controller
    // --------------------------------------------------------

    const char* port() const;
    const char* controllerType() const;
    int baudrate() const;


    // --------------------------------------------------------
    // Opslaan
    // --------------------------------------------------------

    void save(
        const char* port,
        const char* controllerType,
        int baudrate
    );


    // --------------------------------------------------------
    // Wissen
    // --------------------------------------------------------

    void clear();


private:

    Preferences preferences;

    bool preferenceAvailable = false;

    String preferredPort;
    String preferredControllerType;

    int preferredBaudrate = 0;
};


#endif