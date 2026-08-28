#include "MachineMapper.h"

#include <Arduino.h>

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
/*
MachineState is bewust geen onderdeel meer van het
bepalen van de jogafstand.


    De JogPlanner heeft al een relatieve delta voor dit
    tijdslot geleverd.

    We houden machineState in de interface omdat andere
    machinecommando's hem mogelijk later nodig hebben.
*/

(void)machineState;


commandAvailable =
    false;


switch(jog.type)
{
    case JOG_MOVE:

        pendingCommand =
            mapJogMove(
                jog
            );

        break;


    case JOG_CANCEL:

        pendingCommand =
            mapJogCancel();

        break;


    case JOG_NONE:

    default:

        pendingCommand.type =
            MACHINE_COMMAND_NONE;

        pendingCommand.command =
            "";

        break;
}


if(
    pendingCommand.type !=
    MACHINE_COMMAND_NONE
)
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


/*
    De planner heeft al bepaald hoeveel er in dit tijdslot
    moet worden bewogen.

    Er wordt dus NIET meer gekeken naar MachineState.

    De fysieke beweging is het resultaat van het verwerken
    van deze onafhankelijke $J-commando's door GRBL.
*/

char buffer[64];


char axisChar =
    'X';


switch(jog.axis)
{
    case AXIS_X:
        axisChar = 'X';
        break;

    case AXIS_Y:
        axisChar = 'Y';
        break;

    case AXIS_Z:
        axisChar = 'Z';
        break;

    case AXIS_NONE:
    default:

        command.type =
            MACHINE_COMMAND_NONE;

        command.command =
            "";

        return command;
}


/*
    GRBL realtime jogging.

    De tijdsbasis zit in de combinatie:

        delta
        feedrate

    waarbij feedrate door JogPlanner uit de duur van het
    tijdslot is berekend.

    Voorbeeld:

        delta    = 0.010 mm
        duration = 50 ms
        feedrate = 100 mm/min (minimum)

    De $J is relatief en zelfstandig.
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


Serial.println(
    "[MachineMapper] JOG_MOVE"
);

Serial.print(
    "  axis: "
);

Serial.println(
    axisChar
);

Serial.print(
    "  delta: "
);

Serial.println(
    jog.delta,
    4
);

Serial.print(
    "  duration: "
);

Serial.print(
    jog.duration
);

Serial.println(
    " ms"
);

Serial.print(
    "  feedrate: "
);

Serial.println(
    jog.feedrate
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

/*
    GRBL realtime jog cancel.
*/
command.command = "";


Serial.println(
    "[MachineMapper] JOG_CANCEL"
);


return command;


}