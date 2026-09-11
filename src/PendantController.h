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


enum CommandAction
{
    COMMAND_ACTION_NONE,
    COMMAND_ACTION_HOME_X,
    COMMAND_ACTION_HOME_Y,
    COMMAND_ACTION_HOME_Z,
    COMMAND_ACTION_HOME_ALL,
    COMMAND_ACTION_GCODE_STOP
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

    // ============================================================
    // COMPONENTS
    // ============================================================

    Display display;

    InputManager input;

    CNCjsInterface cnc;

    PendantHandlers handlers;

    JogPlanner jogPlanner;

    PendantState pendantState;


    // ============================================================
    // CONTROL ACTION
    // ============================================================

    CommandAction pendingCommandAction =
        COMMAND_ACTION_NONE;

    static constexpr unsigned long CONTROL_CONFIRM_TIMEOUT_MS =
        5000;

    unsigned long commandActionStartedAt =
        0;


    // ============================================================
    // EVENT HANDLING
    // ============================================================

    void handle(
        const Event& event
    );

    void toggleLayer();

    void setLayer(
        PendantLayer layer
    );

    void clearHandlers();


    // ============================================================
    // LAYERS
    // ============================================================

    void enterJogLayer();

    void enterInfoLayer();

    void enterCommandLayer();


    // ============================================================
    // JOG
    // ============================================================

    void initialiseJogPlanner();

    void handleJogEncoder(
        const Event& event
    );

    void feedHoldCycleStart();

    void gcodeStartPause();

    void gcodeStop();

    void homeAxis();

    void zeroAxis();

    void previousWcs();

    void nextWcs();

    void unlock();

    void reset();

    void selectXAxis();

    void selectYAxis();

    void selectZAxis();

    void decreaseJogStep();

    void increaseJogStep();


    // ============================================================
    // CONTROL
    // ============================================================

    void requestHomeX();

    void requestHomeY();

    void requestHomeZ();

    void requestHomeAll();

    void requestGcodeStop();


    void confirmCommandAction();

    void checkCommandActionTimeout();


    // ============================================================
    // DISPLAY STATE
    // ============================================================

    bool displayDirty =
        true;


    CNCjsInterface::CNCjsStatus lastCncStatus =
        CNCjsInterface::CNCjsStatus::Offline;


    MachineStatus lastMachineStatus =
        MACHINE_DISCONNECTED;


    float lastWorkPositionX =
        0.0f;

    float lastWorkPositionY =
        0.0f;

    float lastWorkPositionZ =
        0.0f;


    String lastActiveWcs =
        "";

    String lastJobName =
        "";


    // ============================================================
    // DISPLAY
    // ============================================================

    void updateDisplay();

    void updateNormalDisplay();

    void updateMachineStatus();

    void updateCncStatus();

    void checkStatusChanges();


    const char* layerName() const;

    const char* axisName() const;

    const char* machineStatusName() const;

    const char* cncStatusName() const;

    const char* commandActionName() const;


    const uint8_t* iconForAxis(
        Axis axis
    ) const;
};


#endif