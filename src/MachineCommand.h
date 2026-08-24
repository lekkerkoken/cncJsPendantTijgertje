#ifndef MACHINE_COMMAND_H
#define MACHINE_COMMAND_H

#include <Arduino.h>


enum MachineCommandType
{
    MACHINE_COMMAND_NONE,

    /*
        Voer een concreet G-code/jogcommando uit.
    */
    MACHINE_COMMAND_GCODE,

    /*
        Annuleer een actieve jog.
        De CNCjsInterface vertaalt dit naar de juiste
        CNCjs/controller-functionaliteit.
    */
    MACHINE_COMMAND_JOG_CANCEL,

    /*
        Pauzeer de machinebeweging.
    */
    MACHINE_COMMAND_FEED_HOLD,

    /*
        Hervat een gepauzeerde machinebeweging.
    */
    MACHINE_COMMAND_RESUME,

    /*
        Reset de controller.
    */
    MACHINE_COMMAND_RESET
};

struct MachineCommand
{
    MachineCommandType type =
        MACHINE_COMMAND_NONE;


/*
    Commando voor de machine-interface.

    Dit beschrijft wat de CNCjsInterface van de machine moet uitvoeren.
    De CNCjsInterface vertaalt dit vervolgens naar de concrete
    CNCjs/controller-aanroep.

    Bijvoorbeeld:

        MACHINE_COMMAND_GCODE
            command = "$J=G91 Z0.100 F100"

        MACHINE_COMMAND_JOG_CANCEL
            command is leeg
*/
    String command;
};


#endif