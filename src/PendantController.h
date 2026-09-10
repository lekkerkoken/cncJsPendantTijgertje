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

class PendantController;

struct PendantHandlers
{
    void (PendantController::*key1)() = nullptr;
    void (PendantController::*key2)() = nullptr;
    void (PendantController::*key3)() = nullptr;
    void (PendantController::*key4)() = nullptr;
    void (PendantController::*key5)() = nullptr;
    void (PendantController::*key6)() = nullptr;
    void (PendantController::*key7)() = nullptr;
    void (PendantController::*key8)() = nullptr;
    void (PendantController::*key9)() = nullptr;
    void (PendantController::*encoderPress)() = nullptr;
    void (PendantController::*encoderLongPress)() = nullptr;
    void (PendantController::*encoderPulse)(const Event&) = nullptr;
    void (PendantController::*changedToReady)() = nullptr;

};

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

    PendantHandlers handlers;

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

    void initialiseJogPlanner();

    void handleJogEncoder(
        const Event& event
    );

    void feedHoldCycleStart();

    void homeAxis();

    void selectXAxis();
    void selectYAxis();
    void selectZAxis();

    void decreaseJogStep();
    void increaseJogStep();

    bool displayDirty =
        true;


    CNCjsInterface::CNCjsStatus lastCncStatus =
        CNCjsInterface::CNCjsStatus::Offline;


    MachineStatus lastMachineStatus =
        MACHINE_DISCONNECTED;


    float lastWorkPositionX = 0.0f;
    float lastWorkPositionY = 0.0f;
    float lastWorkPositionZ = 0.0f;

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