#ifndef PENDANT_CONTROLLER_H
#define PENDANT_CONTROLLER_H


#include "Event.h"
#include "Display.h"
#include "PendantState.h"
#include "MachineState.h"
#include "CNCjsInterface.h"
#include "InputManager.h"

#include "JogPlanner.h"



class PendantController
{

public:

    void begin(    
        ButtonMatrix& matrix,
        Encoder& encoder
    );


    void update();


    Axis axis() const;


    float jogStepDistance() const;


private:

    Display display;

    InputManager input;

    CNCjsInterface cnc;

    JogPlanner jogPlanner;

    PendantState pendantState;

    void handle(
        const Event& event
    );

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