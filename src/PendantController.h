#ifndef PENDANT_CONTROLLER_H
#define PENDANT_CONTROLLER_H


#include "Event.h"
#include "Display.h"
#include "PendantState.h"
#include "MachineState.h"
#include "CNCjsInterface.h"

#include "JogPlanner.h"


class PendantController
{

public:

    void begin(
        Display& display,
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


    /*
        Geeft de daadwerkelijk geselecteerde jogafstand
        in millimeters terug.

        De controller vertaalt hiermee de PendantState
        naar een waarde die door de JogPlanner gebruikt kan
        worden zonder dat de planner afhankelijk wordt van
        PendantState of JogStep.
    */
    float jogStepDistance() const;



private:

    Display* display =
        nullptr;

    CNCjsInterface* cnc =
        nullptr;

    JogPlanner jogPlanner;

    PendantState pendantState;

    void toggleLayer();

    void setLayer(
        PendantLayer layer
    );

    void enterJogLayer();

    void handleJogEncoder(
        const Event& event
    );

    // ========================================================
    // DISPLAY STATE
    // ========================================================

    bool displayDirty =
        true;

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