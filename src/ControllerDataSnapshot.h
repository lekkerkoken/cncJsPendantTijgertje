#ifndef CONTROLLER_DATA_SNAPSHOT_H
#define CONTROLLER_DATA_SNAPSHOT_H

#include <Arduino.h>


struct ControllerDataSnapshot
{
    // ========================================================
    // CONTROLLER CONNECTION
    // ========================================================

    bool controllerOpen =
        false;

    bool controllerReady =
        false;


    String controllerPort;

    String controllerType;

    int controllerBaudrate =
        0;


    // ========================================================
    // CONTROLLER EVENTS
    // ========================================================

    /*
        Volledig CNCjs-event.

        Bijvoorbeeld:

        ["Grbl:state", {...}]
    */

    String controllerState;


    /*
        Volledig CNCjs-event.

        Bijvoorbeeld:

        ["controller:settings", {...}]
    */

    String controllerSettings;


    /*
        Laatste ruwe serialport:read.

        Bijvoorbeeld:

        "<Idle|MPos:0.000,0.000,0.000|FS:0,0|Ov:100,100,100>"
    */

    String serialRead;


    // ========================================================
    // MACHINE ACTIVITY
    // ========================================================

    bool machineConnected =
        false;


    unsigned long lastMachineActivity =
        0;


    // ========================================================
    // RESET
    // ========================================================

    void clear()
    {
        controllerOpen =
            false;

        controllerReady =
            false;

        controllerPort =
            "";

        controllerType =
            "";

        controllerBaudrate =
            0;

        controllerState =
            "";

        controllerSettings =
            "";

        serialRead =
            "";

        machineConnected =
            false;

        lastMachineActivity =
            0;
    }
};


#endif