#ifndef JOG_COMMAND_H
#define JOG_COMMAND_H

#include "MachineState.h"
#include "PendantState.h"


enum JogCommandType
{
    JOG_NONE,

    /*
        Start of vervolg van een jogbeweging.
    */
    JOG_MOVE,

    /*
        Annuleer de huidige jogbewefeeging.

        Voor GRBL wordt dit uiteindelijk vertaald naar
        de realtime jog-cancel 0x85.
    */
    JOG_CANCEL
};


struct JogCommand
{
    JogCommandType type = JOG_NONE;

    Axis axis = AXIS_NONE;

    /*
        Gewenste eindpositie in werkcoördinaten.

        Dit is de gebruikershorizon van de JogPlanner.
    */
    float targetPosition = 0.0f;

    /*
        Gewenste jog-feedrate in mm/min.
    */
    int feedrate = 0;
};


#endif