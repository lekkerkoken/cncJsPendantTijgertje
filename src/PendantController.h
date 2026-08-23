#ifndef PENDANT_CONTROLLER_H
#define PENDANT_CONTROLLER_H


#include "Event.h"
#include "Display.h"
#include "PendantState.h"
#include "MachineState.h"
#include "CNCjsClient.h"


class PendantController
{

public:

    void begin(
        Display& display,
        MachineState& machineState,
        CNCjsClient& cnc
    );


    void handle(
        const Event& event
    );


    void update();



private:

    Display* display = nullptr;

    MachineState* machineState = nullptr;

    CNCjsClient* cnc = nullptr;


    PendantState pendantState;


    // ========================================================
    // DISPLAY STATE
    // ========================================================

    bool displayDirty = true;

    CNCjsClient::CNCjsStatus lastCncStatus =
        CNCjsClient::CNCjsStatus::Offline;

    MachineStatus lastMachineStatus =
        MACHINE_DISCONNECTED;


    // ========================================================
    // DISPLAY
    // ========================================================

    void updateDisplay();

    void updateNormalDisplay();

    void updateMachineStatus();

    void updateCncStatus();


    // ========================================================
    // EVENTS
    // ========================================================


    // ========================================================
    // STATUS
    // ========================================================

    void checkStatusChanges();


    // ========================================================
    // NAMES
    // ========================================================

    const char* layerName() const;

    const char* axisName() const;

    const char* machineStatusName() const;

    const char* cncStatusName() const;

};


#endif