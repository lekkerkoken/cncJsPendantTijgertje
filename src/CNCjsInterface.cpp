#include "CNCjsInterface.h"


// ============================================================
// SNAPSHOTS
// ============================================================

CNCjsInterface::CNCjsSnapshot
CNCjsInterface::snapshot() const
{
    return core_.snapshot();
}


MachineState CNCjsInterface::machineStateSnapshot() const
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

    return machineMapper_.map(snapshot);
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
    core_.begin();
}


void CNCjsInterface::update()
{
    if(
        !core_.controllerReady()
    )
    {
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


String CNCjsInterface::port(
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


String CNCjsInterface::controller(
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


String CNCjsInterface::selectedControllerName() const
{
    return core_.selectedControllerName();
}


int CNCjsInterface::selectedPort() const
{
    return core_.selectedPort();
}


String CNCjsInterface::selectedPortName() const
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


void CNCjsInterface::cacheControllerSettings()
{
    MachineSettings settings =
        machineSettingsSnapshot();

    cachedControllerType =
        settings.controllerType;
}


int CNCjsInterface::controllerBaudrate() const
{
    return core_.controllerBaudrate();
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


bool CNCjsInterface::execute(
    const JogCommand& jog
)
{
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
    const String& command
)
{
    return core_.sendCommand(
        command
    );
}


bool CNCjsInterface::jogCancel()
{
    return sendCommand(
        "jogCancel"
    );}


bool CNCjsInterface::feedHold()
{
    return sendCommand(
        "feedhold"
    );
}

bool CNCjsInterface::cyclestart()
{
    return sendCommand(
        "cyclestart"
    );
}

bool CNCjsInterface::start()
{
    return sendCommand(
        "gcode:start"
    );
}


bool CNCjsInterface::pause()
{
    return sendCommand(
        "gcode:pause"
    );
}


bool CNCjsInterface::resume()
{
    return sendCommand(
        "gcode:resume"
    );
}

bool CNCjsInterface::stop()
{
    return sendCommand(
        "gcode:stop"
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