#ifndef CNCJS_INTERFACE_H
#define CNCJS_INTERFACE_H

#include <Arduino.h>

#include "JogCommand.h"
#include "CNCjsClientCore.h"
#include "MachineCommand.h"
#include "MachineState.h"
#include "MachineMapper.h"
#include "Machine.h"

#include "ControllerSettingsSnapshot.h"
#include "ControllerStateSnapshot.h"



class CNCjsInterface
{
public:

    // ========================================================
    // TYPES
    // ========================================================

    using CNCjsStatus =
        CNCjsClientCore::CNCjsStatus;

    using CNCjsSnapshot =
        CNCjsClientCore::CNCjsSnapshot;


    // ========================================================
    // SNAPSHOTS
    // ========================================================

    CNCjsSnapshot snapshot() const;

    MachineState machineStateSnapshot() const;

    MachineSettings machineSettingsSnapshot() const;

    // ========================================================
    // STATUS
    // ========================================================

    /*
        Kept as a lightweight compatibility accessor.

        PendantController currently uses cnc.status().
        Internally this simply reads the Core snapshot.
    */
    CNCjsStatus status() const;


    // ========================================================
    // LIFECYCLE
    // ========================================================

    void begin(
    );

    void update();


    // ========================================================
    // CONNECTION
    // ========================================================

    bool wifiConnected() const;

    bool authenticated() const;

    bool socketConnected() const;


    // ========================================================
    // SERIAL PORTS
    // ========================================================

    int portCount() const;

    String port(
        int index
    ) const;


    // ========================================================
    // CONTROLLERS
    // ========================================================

    int controllerCount() const;

    String controller(
        int index
    ) const;


    // ========================================================
    // CONTROLLER SELECTION
    // ========================================================

    bool controllerSelectionReady() const;

    int selectedController() const;

    String selectedControllerName() const;

    int selectedPort() const;

    String selectedPortName() const;


    bool selectController(
        int portIndex,
        int controllerIndex
    );

    void chooseController();


    // ========================================================
    // ACTIVE CONTROLLER
    // ========================================================

    bool controllerReady() const;

    String controllerPort() const;

    String controllerType() const;

    int controllerBaudrate() const;


    // ========================================================
    // CONTROLLER COMMUNICATION
    // ========================================================

    bool openSelectedController();

    bool openController(
        const char* port,
        const char* controllerType,
        int baudrate
    );


    // ========================================================
    // COMMANDS
    // ========================================================

    bool execute(
        const MachineCommand& command
    );

    bool execute(
    const JogCommand& jog
);

    bool sendGcode(
        const char* gcode
    );

    bool sendGcode(
        const char* port,
        const char* gcode
    );


    bool sendCommand(
        const String& command
    );


    bool jogCancel();

    bool feedHold();

    bool resume();

    bool reset();

    bool sendRealtime(
        uint8_t command
    );


private:

    CNCjsClientCore core_;
    MachineMapper machineMapper_;
    Machine machine_;
};


#endif