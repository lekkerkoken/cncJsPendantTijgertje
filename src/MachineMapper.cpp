#include "MachineMapper.h"

#include <cstdio>

// ============================================================
// MAP
// ============================================================

MachineCommand MachineMapper::map(
    const JogCommand& jog
)
{
    MachineCommand command;


    switch(jog.type)
    {
        case JOG_MOVE:

            return mapJogMove(
                jog
            );


        case JOG_NONE:

        default:

            command.type =
                MACHINE_COMMAND_NONE;

            command.command =
                "";

            return command;
    }
}


// ============================================================
// MAP JOG MOVE
// ============================================================

MachineCommand MachineMapper::mapJogMove(
    const JogCommand& jog
)
{
    MachineCommand command;


    command.type =
        MACHINE_COMMAND_GCODE;


    if(
        jog.axis ==
        AXIS_NONE
    )
    {
        command.type =
            MACHINE_COMMAND_NONE;

        command.command =
            "";

        return command;
    }


    char axisChar =
        'X';


    switch(jog.axis)
    {
        case AXIS_X:

            axisChar =
                'X';

            break;


        case AXIS_Y:

            axisChar =
                'Y';

            break;


        case AXIS_Z:

            axisChar =
                'Z';

            break;


        case AXIS_NONE:

        default:

            command.type =
                MACHINE_COMMAND_NONE;

            command.command =
                "";

            return command;
    }


    char buffer[64];


    /*
        De JogPlanner heeft de beweging al gepland.

        delta:
            relatieve afstand voor dit tijdslot.

        feedrate:
            door de JogPlanner bepaalde snelheid.

        MachineMapper vertaalt dit naar de
        controller-specifieke jog-syntax.
    */

    snprintf(
        buffer,
        sizeof(buffer),
        "$J=G91 %c%.3f F%d",
        axisChar,
        jog.delta,
        jog.feedrate
    );


    command.command =
        buffer;


    return command;
}