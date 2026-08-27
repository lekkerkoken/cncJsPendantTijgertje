#include "MachineMapper.h"


// ============================================================
// BEGIN
// ============================================================

void MachineMapper::begin()
{
    commandAvailable =
        false;

    pendingCommand.type =
        MACHINE_COMMAND_NONE;

    pendingCommand.command =
        "";
}


// ============================================================
// UPDATE
// ============================================================

void MachineMapper::update(
    const JogCommand& jog,
    const MachineState& machineState
)
{
    commandAvailable =
        false;


    switch(jog.type)
    {
        case JOG_MOVE:

            pendingCommand =
                mapJogMove(
                    jog,
                    machineState
                );

            break;


        default:

            pendingCommand.type =
                MACHINE_COMMAND_NONE;

            pendingCommand.command =
                "";

            break;
    }


    if(pendingCommand.type != MACHINE_COMMAND_NONE)
    {
        commandAvailable =
            true;
    }
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
    commandAvailable =
        false;

    return pendingCommand;
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

            G91 X1.000 F1000

        Omdat de jogafstand relatief is, hoeven we hier
        niet zelf de absolute machinepositie te berekenen.

        De JogPlanner levert echter een absolute
        targetPosition aan. Daarom bepalen we hier eerst
        de huidige werkpositie van de actieve as.
    */

    float machinePosition;


    switch(jog.axis)
    {
        case AXIS_X:

            machinePosition =
                machineState.workPosition.x;

            break;


        case AXIS_Y:

            machinePosition =
                machineState.workPosition.y;

            break;


        case AXIS_Z:

            machinePosition =
                machineState.workPosition.z;

            break;


        case AXIS_NONE:

        default:

            command.type =
                MACHINE_COMMAND_NONE;

            command.command =
                "";

            return command;
    }


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
        "  axis: "
    );

    switch(jog.axis)
    {
        case AXIS_X:
            Serial.println("X");
            break;

        case AXIS_Y:
            Serial.println("Y");
            break;

        case AXIS_Z:
            Serial.println("Z");
            break;

        default:
            Serial.println("NONE");
            break;
    }

    Serial.print(
        "  machine position: "
    );

    Serial.println(
        machinePosition,
        3
    );

    Serial.print(
        "  target position: "
    );

    Serial.println(
        jog.targetPosition,
        3
    );

    Serial.print(
        "  distance: "
    );

    Serial.println(
        distance,
        3
    );

    Serial.print(
        "  command: "
    );

    Serial.println(
        command.command
    );


    return command;
}