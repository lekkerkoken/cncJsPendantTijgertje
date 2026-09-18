#include "MachineMapper.h"

#include <cstdio>


// ============================================================
// CONTROLLER TYPE
// ============================================================

ControllerType MachineMapper::controllerTypeFromString(
    const String& type
) const
{
    if(type == "Grbl")
    {
        return CONTROLLER_GRBL;
    }

    if(type == "TinyG")
    {
        return CONTROLLER_TINYG;
    }

    return CONTROLLER_UNKNOWN;
}


// ============================================================
// MAP JOG COMMAND
// ============================================================

String MachineMapper::map(
    const JogCommand& jog,
    ControllerType type
)
{
    switch(jog.type)
    {
        case JOG_MOVE:

            return mapJogMove(
                jog,
                type
            );


        case JOG_NONE:

        default:

            return unsupported();
    }
}


// ============================================================
// MAP CONTROLLER SETTINGS
// ============================================================

MachineSettings MachineMapper::map(
    const ControllerSettingsSnapshot& snapshot
) const
{
    MachineSettings settings;

    if(snapshot.settings.isNull())
    {
        return settings;
    }

    settings.controllerType =
        controllerTypeFromString(
            snapshot.controllerType
        );

    switch(settings.controllerType)
    {
        case CONTROLLER_GRBL:

            settings.maxFeedrate.x =
                snapshot.settings["$110"].as<float>();

            settings.maxFeedrate.y =
                snapshot.settings["$111"].as<float>();

            settings.maxFeedrate.z =
                snapshot.settings["$112"].as<float>();

            break;


        case CONTROLLER_TINYG:

            settings.maxFeedrate.x =
                snapshot.settings["xfr"].as<float>();

            settings.maxFeedrate.y =
                snapshot.settings["yfr"].as<float>();

            settings.maxFeedrate.z =
                snapshot.settings["zfr"].as<float>();

            break;


        case CONTROLLER_UNKNOWN:

        default:

            break;
    }

    return settings;
}


// ============================================================
// MAP CONTROLLER STATE
// ============================================================

MachineState MachineMapper::map(
    const ControllerStateSnapshot& snapshot,
    bool& valid
) const
{
    valid = false;

    MachineState state;

    if(snapshot.state.isNull())
    {
        return state;
    }

    ControllerType type =
        controllerTypeFromString(
            snapshot.controllerType
        );

    switch(type)
    {
        case CONTROLLER_GRBL:
        {
            JsonObjectConst status =
                snapshot.state["status"].as<JsonObjectConst>();

            if(status.isNull())
            {
                return state;
            }

            String activeState =
                status["activeState"].as<String>();

            if(activeState == "Idle")
            {
                state.machineStatus =
                    MACHINE_IDLE;
            }
            else if(activeState == "Run")
            {
                state.machineStatus =
                    MACHINE_RUN;
            }
            else if(activeState == "Hold")
            {
                state.machineStatus =
                    MACHINE_HOLD;
            }
            else if(activeState == "Alarm")
            {
                state.machineStatus =
                    MACHINE_ALARM;
            }
            else
            {
                return state;
            }

            JsonObjectConst machinePosition =
                status["mpos"].as<JsonObjectConst>();

            JsonObjectConst workPosition =
                status["wpos"].as<JsonObjectConst>();

            if(
                machinePosition.isNull() ||
                workPosition.isNull()
            )
            {
                return state;
            }

            state.machinePosition.x =
                machinePosition["x"].as<float>();

            state.machinePosition.y =
                machinePosition["y"].as<float>();

            state.machinePosition.z =
                machinePosition["z"].as<float>();

            state.workPosition.x =
                workPosition["x"].as<float>();

            state.workPosition.y =
                workPosition["y"].as<float>();

            state.workPosition.z =
                workPosition["z"].as<float>();

            state.activeWcs =
                snapshot.state["parserstate"]["modal"]["wcs"]
                    .as<String>();

            valid = true;

            break;
        }


        case CONTROLLER_TINYG:
        {
            JsonObjectConst status =
                snapshot.state["sr"].as<JsonObjectConst>();

            if(status.isNull())
            {
                return state;
            }

            int machineState =
                status["machineState"].as<int>();

            switch(machineState)
            {
                case 1:
                case 3:
                case 4:

                    state.machineStatus =
                        MACHINE_IDLE;

                    break;


                case 2:
                case 11:

                    state.machineStatus =
                        MACHINE_ALARM;

                    break;


                case 5:
                case 7:
                case 8:
                case 9:
                case 10:

                    state.machineStatus =
                        MACHINE_RUN;

                    break;


                case 6:

                    state.machineStatus =
                        MACHINE_HOLD;

                    break;


                default:

                    return state;
            }

            JsonObjectConst machinePosition =
                status["mpos"].as<JsonObjectConst>();

            JsonObjectConst workPosition =
                status["wpos"].as<JsonObjectConst>();

            if(
                machinePosition.isNull() ||
                workPosition.isNull()
            )
            {
                return state;
            }

            state.machinePosition.x =
                machinePosition["x"].as<float>();

            state.machinePosition.y =
                machinePosition["y"].as<float>();

            state.machinePosition.z =
                machinePosition["z"].as<float>();

            state.workPosition.x =
                workPosition["x"].as<float>();

            state.workPosition.y =
                workPosition["y"].as<float>();

            state.workPosition.z =
                workPosition["z"].as<float>();

            state.feedrate =
                status["feedrate"].as<float>();

            state.spindleSpeed =
                status["spd"].as<int>();

            state.activeWcs =
                status["coor"].as<String>();

            valid = true;

            break;
        }


        case CONTROLLER_UNKNOWN:

        default:

            break;
    }

    return state;
}


// ============================================================
// MAP JOG MOVE
// ============================================================

String MachineMapper::mapJogMove(
    const JogCommand& jog,
    ControllerType type
)
{
    switch(type)
    {
        case CONTROLLER_GRBL:

            return mapGrblJog(
                jog
            );


        case CONTROLLER_TINYG:

            // TinyG jog mapping

            return unsupported();


        case CONTROLLER_UNKNOWN:

        default:

            return unsupported();
    }
}


// ============================================================
// MAP GRBL JOG
// ============================================================

String MachineMapper::mapGrblJog(
    const JogCommand& jog
)
{
    if(
        jog.axis ==
        AXIS_NONE
    )
    {
        return unsupported();
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

            return unsupported();
    }

    char buffer[64];

    /*
        De JogPlanner heeft de beweging al gepland.

        delta:
            relatieve afstand voor dit tijdslot.

        feedrate:
            door de JogPlanner bepaalde snelheid.
    */

    snprintf(
        buffer,
        sizeof(buffer),
        "$J=G91 %c%.3f F%d",
        axisChar,
        jog.delta,
        jog.feedrate
    );

    return String(buffer);
}


// ============================================================
// MAP HOME
// ============================================================

String MachineMapper::mapHome(
    Axis axis,
    ControllerType type
)
{
    switch(type)
    {
        case CONTROLLER_GRBL:

            /*
                Standaard GRBL ondersteunt geen individuele
                axis-homing via bijvoorbeeld "$H X".

                Daarom voorlopig unsupported.
            */

            return unsupported();


        case CONTROLLER_TINYG:

            // TinyG home mapping

            return unsupported();


        case CONTROLLER_UNKNOWN:

        default:

            return unsupported();
    }
}


// ============================================================
// MAP ZERO WCS AXIS
// ============================================================

String MachineMapper::mapZeroAxis(
    Axis axis,
    ControllerType type
)
{
    switch(type)
    {
        case CONTROLLER_GRBL:
        {
            /*
                Stel de huidige positie van de geselecteerde
                as in als WCS-coördinaat 0.

                G10 L20 gebruikt de huidige positie als
                uitgangspunt en schrijft daarmee de
                werkcoördinaat-offset.

                P0 = actieve work coordinate system.
            */

            char axisChar;

            switch(axis)
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

                    return unsupported();
            }

            char buffer[32];

            snprintf(
                buffer,
                sizeof(buffer),
                "G10 L20 P0 %c0",
                axisChar
            );

            return String(buffer);
        }


        case CONTROLLER_TINYG:

            // TinyG zero-axis mapping

            return unsupported();


        case CONTROLLER_UNKNOWN:

        default:

            return unsupported();
    }
}


// ============================================================
// UNSUPPORTED
// ============================================================

String MachineMapper::unsupported() const
{
    return "";
}