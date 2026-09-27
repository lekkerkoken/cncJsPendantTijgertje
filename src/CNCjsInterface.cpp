#include "CNCjsInterface.h"


// ============================================================
// SNAPSHOTS
// ============================================================

CNCjsInterface::CNCjsSnapshot
CNCjsInterface::snapshot() const
{
    return core_.snapshot();
}


MachineState
CNCjsInterface::machineStateSnapshot() const
{
    return machine_.state;
}


MachineSettings
CNCjsInterface::machineSettingsSnapshot() const
{
    MachineSettings settings;

    ControllerSettingsSnapshot snapshot =
        core_.controllerSettingsSnapshot();

    if(
        !snapshot.valid
    )
    {
        return settings;
    }

    settings =
        machineMapper_.map(
            snapshot
        );

    return settings;
}


JobSnapshot
CNCjsInterface::jobSnapshot() const
{
    return core_.jobSnapshot();
}


// ============================================================
// STATUS
// ============================================================

CNCjsInterface::CNCjsStatus
CNCjsInterface::status() const
{
    return core_.status();
}


// ============================================================
// LIFECYCLE
// ============================================================

void CNCjsInterface::begin()
{
    core_.setSerialportReadHandler(
        [this](const JsonArray& array)
        {
            handleSerialportRead(
                array
            );
        }
    );

    core_.setControllerOpeningHandler(
        [this](const char* controllerType)
        {
            handleControllerOpening(
                controllerType
            );
        }
    );

    core_.begin();
}


void CNCjsInterface::update()
{
    if(
        !core_.controllerReady()
    )
    {
        controllerSettingsPublished();

        machine_.state.machineStatus =
            MACHINE_DISCONNECTED;

        return;
    }

    ControllerStateSnapshot snapshot =
        core_.controllerStateSnapshot();

    if(
        !snapshot.valid
    )
    {
        return;
    }

    bool valid = false;

    MachineState state =
        machineMapper_.map(
            snapshot,
            valid
        );

    if(valid)
    {
        machine_.state =
            state;
    }
}


// ============================================================
// CONNECTION
// ============================================================

bool CNCjsInterface::wifiConnected() const
{
    return core_.wifiConnected();
}


bool CNCjsInterface::authenticated() const
{
    return core_.authenticated();
}


bool CNCjsInterface::socketConnected() const
{
    return core_.socketConnected();
}


// ============================================================
// SERIAL PORTS
// ============================================================

int CNCjsInterface::portCount() const
{
    return core_.portCount();
}


String
CNCjsInterface::port(
    int index
) const
{
    return core_.port(
        index
    );
}


// ============================================================
// CONTROLLERS
// ============================================================

int CNCjsInterface::controllerCount() const
{
    return core_.controllerCount();
}


String
CNCjsInterface::controller(
    int index
) const
{
    return core_.controller(
        index
    );
}


// ============================================================
// CONTROLLER SELECTION
// ============================================================

bool CNCjsInterface::controllerSelectionReady() const
{
    return core_.controllerSelectionReady();
}


int CNCjsInterface::selectedController() const
{
    return core_.selectedController();
}


String
CNCjsInterface::selectedControllerName() const
{
    return core_.selectedControllerName();
}


int CNCjsInterface::selectedPort() const
{
    return core_.selectedPort();
}


String
CNCjsInterface::selectedPortName() const
{
    return core_.selectedPortName();
}


bool CNCjsInterface::selectController(
    int portIndex,
    int controllerIndex
)
{
    return core_.selectController(
        portIndex,
        controllerIndex
    );
}


void CNCjsInterface::chooseController()
{
    core_.chooseController();
}


// ============================================================
// ACTIVE CONTROLLER
// ============================================================

bool CNCjsInterface::controllerReady() const
{
    return core_.controllerReady();
}


int CNCjsInterface::controllerBaudrate() const
{
    return core_.controllerBaudrate();
}


// ============================================================
// CONTROLLER OPENING
// ============================================================

void CNCjsInterface::handleControllerOpening(
    const char* controllerType
)
{
    if(
        controllerType == nullptr
    )
    {
        cachedControllerType =
            CONTROLLER_UNKNOWN;

        return;
    }


    cachedControllerType =
        controllerTypeFromString(
            String(controllerType)
        );
}


ControllerType
CNCjsInterface::controllerTypeFromString(
    const String& controllerType
) const
{
    if(
        controllerType.equals(
            "Grbl"
        )
    )
    {
        return CONTROLLER_GRBL;
    }


    if(
        controllerType.equals(
            "TinyG"
        )
    )
    {
        return CONTROLLER_TINYG;
    }


    return CONTROLLER_UNKNOWN;
}


// ============================================================
// CONTROLLER SETTINGS PUBLICATION
// ============================================================

void CNCjsInterface::controllerSettingsPublished()
{
    uint32_t publicationId =
        core_.controllerSettingsPublicationId();


    if(
        publicationId ==
        handledControllerSettingsPublicationId_
    )
    {
        return;
    }


    handledControllerSettingsPublicationId_ =
        publicationId;


    ControllerSettingsSnapshot snapshot =
        core_.controllerSettingsSnapshot();


    if(
        !core_.handleControllerIdentification(
            snapshot.controllerType.c_str()
        )
    )
    {
        return;
    }


    switch(
        machineMapper_.settingsState(
            snapshot
        )
    )
    {
        case MachineMapper::CONTROLLER_DECLARATION_ONLY:

            // Controller is known.
            // Controller-specific settings are not available yet.

            break;


        case MachineMapper::CONTROLLER_SETTINGS_AVAILABLE:

            core_.confirmControllerSettingsPublished();

            break;
    }
}


// ============================================================
// SERIAL DATA
// ============================================================

void CNCjsInterface::handleSerialportRead(
    const JsonArray& array
)
{
    MachineData data;

    if(
        !machineMapper_.mapSerialData(
            array,
            cachedControllerType,
            data
        )
    )
    {
        return;
    }


    consumeMachineData(
        data
    );
}


void CNCjsInterface::consumeMachineData(
    const MachineData& data
)
{
    switch(data.type)
    {
        case MachineData::MACHINE_DATA_SETTINGS:
        {
            MachineSettings defaultSettings;


            if(
                data.settings.maxFeedrate.x !=
                defaultSettings.maxFeedrate.x
            )
            {
                machine_.settings.maxFeedrate.x =
                    data.settings.maxFeedrate.x;
            }


            if(
                data.settings.maxFeedrate.y !=
                defaultSettings.maxFeedrate.y
            )
            {
                machine_.settings.maxFeedrate.y =
                    data.settings.maxFeedrate.y;
            }


            if(
                data.settings.maxFeedrate.z !=
                defaultSettings.maxFeedrate.z
            )
            {
                machine_.settings.maxFeedrate.z =
                    data.settings.maxFeedrate.z;
            }


            if(
                data.settings.controllerType !=
                defaultSettings.controllerType
            )
            {
                machine_.settings.controllerType =
                    data.settings.controllerType;
            }

            break;
        }


        case MachineData::MACHINE_DATA_STATE:
        {
            // Machine state received through serial data.
            //
            // Consumption will be implemented when serial
            // state data becomes relevant.

            break;
        }


        case MachineData::MACHINE_DATA_NONE:
        default:
            break;
    }
}


// ============================================================
// CONTROLLER SETTINGS CACHE
// ============================================================

void CNCjsInterface::cacheControllerSettings()
{
    MachineSettings settings =
        machineSettingsSnapshot();

    cachedControllerType =
        settings.controllerType;
}


// ============================================================
// CONTROLLER COMMUNICATION
// ============================================================

bool CNCjsInterface::openSelectedController()
{
    return core_.openSelectedController();
}


bool CNCjsInterface::openController(
    const char* port,
    const char* controllerType,
    int baudrate
)
{
    return core_.openController(
        port,
        controllerType,
        baudrate
    );
}


// ============================================================
// COMMANDS
// ============================================================

bool CNCjsInterface::homeAxis(
    Axis axis
)
{
    String gcode =
        machineMapper_.mapHome(
            axis,
            cachedControllerType
        );

    return sendGcode(
        gcode.c_str()
    );
}


bool CNCjsInterface::unlock()
{
    return sendCommand(
        "unlock"
    );
}


bool CNCjsInterface::reset()
{
    return sendCommand(
        "reset"
    );
}


bool CNCjsInterface::zeroAxis(
    Axis axis
)
{
    String gcode =
        machineMapper_.mapZeroAxis(
            axis,
            cachedControllerType
        );

    return sendGcode(
        gcode.c_str()
    );
}


bool CNCjsInterface::nextWcs()
{
    MachineState state =
        machineStateSnapshot();

    String nextWcs;

    if(state.activeWcs == "G54")
        nextWcs = "G55";
    else if(state.activeWcs == "G55")
        nextWcs = "G56";
    else if(state.activeWcs == "G56")
        nextWcs = "G57";
    else if(state.activeWcs == "G57")
        nextWcs = "G58";
    else if(state.activeWcs == "G58")
        nextWcs = "G59";
    else
        nextWcs = "G59";

    return sendGcode(
        nextWcs
    );
}


bool CNCjsInterface::previousWcs()
{
    MachineState state =
        machineStateSnapshot();

    String previousWcs;

    if(state.activeWcs == "G59")
        previousWcs = "G58";
    else if(state.activeWcs == "G58")
        previousWcs = "G57";
    else if(state.activeWcs == "G57")
        previousWcs = "G56";
    else if(state.activeWcs == "G56")
        previousWcs = "G55";
    else if(state.activeWcs == "G55")
        previousWcs = "G54";
    else
        previousWcs = "G54";

    return sendGcode(
        previousWcs
    );
}


bool CNCjsInterface::execute(
    const JogCommand& jog
)
{
    if(
        jog.type == JOG_CANCEL
    )
    {
        return jogCancel();
    }

    String gcode =
        machineMapper_.map(
            jog,
            cachedControllerType
        );

    return sendGcode(
        gcode.c_str()
    );
}


bool CNCjsInterface::sendGcode(
    const String& gcode
)
{
    if(
        gcode.length() == 0
    )
    {
        return false;
    }

    return core_.sendGcode(
        gcode.c_str()
    );
}


bool CNCjsInterface::sendCommand(
    const String& command,
    const JsonObjectConst& options
)
{
    return core_.sendCommand(
        command,
        options
    );
}


bool CNCjsInterface::jogCancel()
{
    return sendCommand(
        "jogCancel"
    );
}


bool CNCjsInterface::feedHold()
{
    return sendCommand(
        "feedhold"
    );
}


bool CNCjsInterface::homeAll()
{
    String gcode =
        machineMapper_.mapHomeAll(
            cachedControllerType
        );

    return sendGcode(
        gcode.c_str()
    );
}


bool CNCjsInterface::cyclestart()
{
    return sendCommand(
        "cyclestart"
    );
}


bool CNCjsInterface::gcodeStart()
{
    return sendCommand(
        "gcode:start"
    );
}


bool CNCjsInterface::gcodePause()
{
    return sendCommand(
        "gcode:pause"
    );
}


bool CNCjsInterface::gcodeResume()
{
    return sendCommand(
        "gcode:resume"
    );
}


bool CNCjsInterface::gcodeStop()
{
    JsonDocument options;

    options["force"] =
        true;

    return sendCommand(
        "gcode:stop",
        options.as<JsonObjectConst>()
    );
}


bool CNCjsInterface::sendRealtime(
    uint8_t command
)
{
    return core_.sendRealtime(
        command
    );
}