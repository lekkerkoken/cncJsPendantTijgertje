#ifndef CNCJS_INTERFACE_H
#define CNCJS_INTERFACE_H

#include <Arduino.h>

#include "CNCjsClientCore.h"
#include "MachineCommand.h"
#include "MachineState.h"


class CNCjsInterface
{
public:

    // ========================================================
    // Status
    // ========================================================

    using CNCjsStatus =
        CNCjsClientCore::CNCjsStatus;


    CNCjsStatus status() const;


    // ========================================================
    // Lifecycle
    // ========================================================

    void begin(
        MachineState& machineState
    );

    void update();


    // ========================================================
    // Connection
    // ========================================================

    bool wifiConnected() const;
    bool authenticated() const;
    bool socketConnected() const;


    // ========================================================
    // Serial ports
    // ========================================================

    int portCount() const;

    const char* port(
        int index
    ) const;


    // ========================================================
    // Controllers
    // ========================================================

    int controllerCount() const;

    const char* controller(
        int index
    ) const;


    // ========================================================
    // Controller selection
    // ========================================================

    bool controllerSelectionReady() const;

    int selectedController() const;

    const char* selectedControllerName() const;

    int selectedPort() const;

    const char* selectedPortName() const;


    bool selectController(
        int portIndex,
        int controllerIndex
    );

    void chooseController();


    // ========================================================
    // Active controller
    // ========================================================

    bool controllerReady() const;

    const char* controllerPort() const;

    const char* controllerType() const;

    int controllerBaudrate() const;


    // ========================================================
    // Controller communication
    // ========================================================

    bool openSelectedController();

    bool openController(
        const char* port,
        const char* controllerType,
        int baudrate
    );

    bool execute(
        const MachineCommand& command
    );


    // ========================================================
    // G-code
    // ========================================================

    bool sendGcode(
        const char* gcode
    );

    bool sendGcode(
        const char* port,
        const char* gcode
    );


    // ========================================================
    // CNCjs controller commands
    // ========================================================

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
};


#endif