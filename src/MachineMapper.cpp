#include "MachineMapper.h"

#include <Arduino.h>


// ============================================================
// BEGIN
// ============================================================

void MachineMapper::begin()
{
    pendingCommand =
        MachineCommand();

    commandAvailable = false;
}


// ============================================================
// UPDATE
// ============================================================

void MachineMapper::update(
    const JogCommand& jog,
    const MachineState& machineState
)
{
    /*
        Slechts één command tegelijk.
    */

    if(commandAvailable)
    {
        return;
    }


    switch(jog.type)
    {
        case JOG_MOVE:

            pendingCommand =
                mapJogMove(
                    jog,
                    machineState
                );

            commandAvailable = true;

            break;


        case JOG_CANCEL:

            pendingCommand =
                mapJogCancel();

            commandAvailable = true;

            break;


        case JOG_NONE:

        default:

            break;
    }
}


// ============================================================
// MAP JOG MOVE
// ============================================================

MachineCommand MachineMapper::mapJogMove(
    const JogCommand& jog,
    const MachineState& machineState
)
{
    MachineCommand command;

    command.type =
        MACHINE_COMMAND_GCODE;


    /*
        Voorlopig GRBL.

        We gebruiken relatieve jogging:

            $J=G91 X1.000 F1000

        Omdat de jogafstand relatief is, hoeven we hier
        niet zelf de absolute machinepositie te berekenen.
    */

    float machinePosition =
        machineState.workPosition.x;


    float distance =
        jog.targetPosition -
        machinePosition;


    char buffer[64];


    switch(jog.axis)
    {
        case AXIS_X:

            snprintf(
                buffer,
                sizeof(buffer),
                "G91 X%.3f F%d",
                distance,
                jog.feedrate
            );

            break;


        case AXIS_Y:

            snprintf(
                buffer,
                sizeof(buffer),
                "G91 Y%.3f F%d",
                distance,
                jog.feedrate
            );

            break;


        case AXIS_Z:

            snprintf(
                buffer,
                sizeof(buffer),
                "G91 Z%.3f F%d",
                distance,
                jog.feedrate
            );

            break;


        case AXIS_NONE:

        default:

            command.type =
                MACHINE_COMMAND_NONE;

            command.command =
                "";

            return command;
    }


    command.command =
        buffer;


    Serial.println(
        "[MachineMapper] JOG_MOVE"
    );

    Serial.print(
        "  command: "
    );

    Serial.println(
        command.command
    );


    return command;
}


// ============================================================
// MAP JOG CANCEL
// ============================================================

MachineCommand MachineMapper::mapJogCancel()
{
    MachineCommand command;

    command.type =
        MACHINE_COMMAND_JOG_CANCEL;

    command.command =
        "";


    Serial.println(
        "[MachineMapper] JOG_CANCEL"
    );


    return command;
}


// ============================================================
// AVAILABLE
// ============================================================

bool MachineMapper::available()
{
    return commandAvailable;
}


// ============================================================
// READ
// ============================================================

MachineCommand MachineMapper::read()
{
    MachineCommand result =
        pendingCommand;

    pendingCommand =
        MachineCommand();

    commandAvailable = false;

    return result;
}