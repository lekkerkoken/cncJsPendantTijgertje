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
    
    return core_.machineStateSnapshot();
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

void CNCjsInterface::begin(
    MachineState& machineState
)
{
    /*
        MachineState is retained in the public interface for
        compatibility with the existing application.

        The Core owns the actual CNCjs machine state.

        The supplied MachineState is therefore no longer used
        as shared state.
    */

    (void)machineState;

    core_.begin();
}


void CNCjsInterface::update()
{
    core_.update();
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


String CNCjsInterface::controllerPort() const
{
    return core_.controllerPort();
}


String CNCjsInterface::controllerType() const
{
    return core_.controllerType();
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


// ============================================================
// COMMANDS
// ============================================================

bool CNCjsInterface::execute(
    const MachineCommand& command
)
{
    return core_.execute(
        command
    );
}


bool CNCjsInterface::sendGcode(
    const char* gcode
)
{
    return core_.sendGcode(
        gcode
    );
}


bool CNCjsInterface::sendGcode(
    const char* port,
    const char* gcode
)
{
    return core_.sendGcode(
        port,
        gcode
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
    return core_.jogCancel();
}


bool CNCjsInterface::feedHold()
{
    return core_.feedHold();
}


bool CNCjsInterface::resume()
{
    return core_.resume();
}


bool CNCjsInterface::reset()
{
    return core_.reset();
}


bool CNCjsInterface::sendRealtime(
    uint8_t command
)
{
    return core_.sendRealtime(
        command
    );
}