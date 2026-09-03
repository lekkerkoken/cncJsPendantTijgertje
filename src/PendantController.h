#ifndef PENDANT_CONTROLLER_H
#define PENDANT_CONTROLLER_H

#include "ButtonMatrix.h"
#include "Encoder.h"
#include "Display.h"
#include "OLED.h"
#include "InputManager.h"
#include "CNCjsInterface.h"
#include "JogPlanner.h"
#include "PendantState.h"


class PendantController
{
public:

    void begin(
        ButtonMatrix& matrix,
        Encoder& encoder,
        OLED& oled
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


    bool displayDirty =
        true;


    CNCjsInterface::CNCjsStatus lastCncStatus =
        CNCjsInterface::CNCjsStatus::Offline;


    MachineStatus lastMachineStatus =
        MACHINE_DISCONNECTED;


    void updateDisplay();

    void updateNormalDisplay();

    void updateMachineStatus();

    void updateCncStatus();

    void checkStatusChanges();


    const char* layerName() const;

    const char* axisName() const;

    const char* machineStatusName() const;

    const char* cncStatusName() const;

    String lastActiveWcs = "";

    const uint8_t* iconForAxis(
        Axis axis
    ) const;
};

#endif