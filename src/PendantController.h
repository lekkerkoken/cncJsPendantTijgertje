#ifndef PENDANT_CONTROLLER_H
#define PENDANT_CONTROLLER_H


#include "Event.h"
#include "Display.h"
#include "PendantState.h"
#include "MachineState.h"
#include "CNCjsInterface.h"


class PendantController
{

public:

    void begin(
        Display& display,
        MachineState& machineState,
        CNCjsInterface& cnc
    );


    void handle(
        const Event& event
    );


    void update();


    /*
        Geeft de momenteel geselecteerde jog-as terug.

        De JogPlanner gebruikt deze waarde om te bepalen
        welke machine-as door de encoder wordt bestuurd.
    */
    Axis axis() const;



private:

    Display* display = nullptr;

    MachineState* machineState = nullptr;

    CNCjsInterface* cnc = nullptr;


    PendantState pendantState;


    // ========================================================
    // DISPLAY STATE
    // ========================================================

    bool displayDirty = true;

    CNCjsInterface::CNCjsStatus lastCncStatus =
        CNCjsInterface::CNCjsStatus::Offline;

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