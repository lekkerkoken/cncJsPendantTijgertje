#include "CNCjsInterface.h"


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
    core_.begin(
        machineState
    );
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


const char*
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


const char*
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


const char*
CNCjsInterface::selectedControllerName() const
{
    return core_.selectedControllerName();
}


int CNCjsInterface::selectedPort() const
{
    return core_.selectedPort();
}


const char*
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


const char*
CNCjsInterface::controllerPort() const
{
    return core_.controllerPort();
}


const char*
CNCjsInterface::controllerType() const
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


bool CNCjsInterface::execute(
    const MachineCommand& command
)
{
    return core_.execute(
        command
    );
}


// ============================================================
// G-CODE
// ============================================================

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


// ============================================================
// CNCJS CONTROLLER COMMANDS
// ============================================================

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