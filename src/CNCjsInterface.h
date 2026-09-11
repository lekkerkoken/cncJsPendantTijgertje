#ifndef CNCJS_INTERFACE_H
#define CNCJS_INTERFACE_H

#include <Arduino.h>

#include "JogCommand.h"
#include "CNCjsClientCore.h"
#include "MachineState.h"
#include "MachineMapper.h"
#include "Machine.h"
#include "PendantState.h"

#include "ControllerSettingsSnapshot.h"
#include "ControllerStateSnapshot.h"
#include "SenderStatusSnapshot.h"


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

    JobSnapshot jobSnapshot() const;


    // ========================================================
    // STATUS
    // ========================================================

    CNCjsStatus status() const;


    // ========================================================
    // LIFECYCLE
    // ========================================================

    void begin();

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
        const JogCommand& jog
    );

    bool sendGcode(
        const String& gcode
    );

    // bool sendGcode(
    //     const char* port,
    //     const char* gcode
    // );

    bool sendCommand(
        const String& command
    );

    bool jogCancel();

    bool homeAxis(
        Axis axis
    );

    bool homeAll();

    bool unlock();

    bool reset();

    bool zeroAxis(
        Axis axis
    );

    bool feedHold();

    bool cyclestart();

    bool start();

    bool pause();

    bool resume();

    bool stop();

    bool sendRealtime(
        uint8_t command
    );

    void cacheControllerSettings();


private:

    CNCjsClientCore core_;
    MachineMapper machineMapper_;
    Machine machine_;

    ControllerType cachedControllerType =
        CONTROLLER_UNKNOWN;
};


#endif