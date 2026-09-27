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
// MAP SERIAL DATA
// ============================================================

bool MachineMapper::mapSerialData(
    const JsonArray& data,
    ControllerType type,
    MachineData& result
) const
{
    result.type =
        MachineData::MACHINE_DATA_NONE;

    switch(type)
    {
        case CONTROLLER_TINYG:
        {
            if(data.isNull())
            {
                return false;
            }

            JsonObjectConst response =
                data[0].as<JsonObjectConst>();

            if(response.isNull())
            {
                return false;
            }

            JsonObjectConst values =
                response["r"].as<JsonObjectConst>();

            if(values.isNull())
            {
                return false;
            }

            bool settingsReceived = false;

            if(values["xfr"].is<float>() || values["xfr"].is<int>())
            {
                result.settings.maxFeedrate.x =
                    values["xfr"].as<float>();

                settingsReceived = true;
            }

            if(values["yfr"].is<float>() || values["yfr"].is<int>())
            {
                result.settings.maxFeedrate.y =
                    values["yfr"].as<float>();

                settingsReceived = true;
            }

            if(values["zfr"].is<float>() || values["zfr"].is<int>())
            {
                result.settings.maxFeedrate.z =
                    values["zfr"].as<float>();

                settingsReceived = true;
            }
            
            if(!settingsReceived)
            {
                return false;
            }

            result.type =
                MachineData::MACHINE_DATA_SETTINGS;

            return true;
        }

        case CONTROLLER_GRBL:
        case CONTROLLER_UNKNOWN:
        default:
            return false;
    }
}

    // ========================================================
    // CONTROLLER INITIALISATION AFTER IDENTIFICATION
    // ========================================================

int MachineMapper::mapControllerInitialisation(
    ControllerType type,
    SerialRequest* requests
) const
{
    if(requests == nullptr)
    {
        return 0;
    }

    switch(type)
    {
        case CONTROLLER_GRBL:
            requests[0].data = "$#\r";
            return 1;

        case CONTROLLER_TINYG:
            requests[0].data = "{\"xfr\":null}\r";
            requests[1].data = "{\"yfr\":null}\r";
            requests[2].data = "{\"zfr\":null}\r";
            return 3;

        case CONTROLLER_UNKNOWN:
        default:
            return 0;
    }
}

// ============================================================
// CONTROLLER SETTINGS STATE
// ============================================================

MachineMapper::SettingsState
MachineMapper::settingsState(
    const ControllerSettingsSnapshot& snapshot
) const
{
    ControllerType controllerType =
        controllerTypeFromString(
            snapshot.controllerType
        );

    switch(controllerType)
    {
        case CONTROLLER_GRBL:
        {
            if(
                snapshot.payload["settings"]["$110"].is<String>() &&
                snapshot.payload["settings"]["$111"].is<String>() &&
                snapshot.payload["settings"]["$112"].is<String>()
            )
            {
                return CONTROLLER_SETTINGS_AVAILABLE;
            }

            return CONTROLLER_DECLARATION_ONLY;
        }

        case CONTROLLER_TINYG:
        {
            if(
                snapshot.payload["mfo"].is<int>() &&
                snapshot.payload["mto"].is<int>() &&
                snapshot.payload["sso"].is<int>()
            )
            {
                return CONTROLLER_SETTINGS_AVAILABLE;
            }

            return CONTROLLER_DECLARATION_ONLY;
        }

        case CONTROLLER_UNKNOWN:
        default:
            return CONTROLLER_DECLARATION_ONLY;
    }
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
            return mapJogMove(jog, type);

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

    if(snapshot.payload.isNull())
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
                snapshot.payload["settings"]["$110"].as<float>();
            settings.maxFeedrate.y =
                snapshot.payload["settings"]["$111"].as<float>();
            settings.maxFeedrate.z =
                snapshot.payload["settings"]["$112"].as<float>();
            break;

        case CONTROLLER_TINYG:
            settings.maxFeedrate.x =
                snapshot.payload["xfr"].as<float>();
            settings.maxFeedrate.y =
                snapshot.payload["yfr"].as<float>();
            settings.maxFeedrate.z =
                snapshot.payload["zfr"].as<float>();
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
                state.machineStatus = MACHINE_IDLE;
            }
            else if(activeState == "Run")
            {
                state.machineStatus = MACHINE_RUN;
            }
            else if(activeState == "Hold")
            {
                state.machineStatus = MACHINE_HOLD;
            }
            else if(activeState == "Alarm")
            {
                state.machineStatus = MACHINE_ALARM;
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
                    state.machineStatus = MACHINE_IDLE;
                    break;

                case 2:
                case 11:
                    state.machineStatus = MACHINE_ALARM;
                    break;

                case 5:
                case 7:
                case 8:
                case 9:
                case 10:
                    state.machineStatus = MACHINE_RUN;
                    break;

                case 6:
                    state.machineStatus = MACHINE_HOLD;
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
            return mapGrblJog(jog);

        case CONTROLLER_TINYG:
            return mapTinyGJog(jog);

        case CONTROLLER_UNKNOWN:
        default:
            return unsupported();
    }
}

String MachineMapper::mapTinyGJog(
    const JogCommand& jog
)
{
    if(jog.axis == AXIS_NONE)
    {
        return unsupported();
    }

    char axisChar;

    switch(jog.axis)
    {
        case AXIS_X:
            axisChar = 'x';
            break;

        case AXIS_Y:
            axisChar = 'y';
            break;

        case AXIS_Z:
            axisChar = 'z';
            break;

        case AXIS_NONE:
        default:
            return unsupported();
    }

    char buffer[32];

    snprintf(
        buffer,
        sizeof(buffer),
        "{\"jog%c\":%.3f}\r",
        axisChar,
        jog.delta
    );

    return String(buffer);
}

// ============================================================
// MAP GRBL JOG
// ============================================================

String MachineMapper::mapGrblJog(
    const JogCommand& jog
)
{
    if(jog.axis == AXIS_NONE)
    {
        return unsupported();
    }

    char axisChar = 'X';

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
            switch(axis)
            {
                case AXIS_X:
                    return "G28.2X0";
                case AXIS_Y:
                    return "G28.2Y0";
                case AXIS_Z:
                    return "G28.2Z0";
                case AXIS_NONE:
                default:
                    return unsupported();
            }

        case CONTROLLER_UNKNOWN:
        default:
            return unsupported();
    }
}

String MachineMapper::mapHomeAll(
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
            return "G28.2X0Y0Z0";

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