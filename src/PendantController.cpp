#include "PendantController.h"

#include <Arduino.h>


// ============================================================
// BEGIN
// ============================================================

void PendantController::begin(
    Display& display,
    CNCjsInterface& cnc
)
{
    this->display =
        &display;


    this->cnc =
        &cnc;


    /*
        Forceer één eerste display-update.
    */

    displayDirty =
        true;


    lastCncStatus =
        cnc.status();


    lastMachineStatus =
        cnc.machineStateSnapshot().machineStatus;


    updateDisplay();
}



// ============================================================
// HANDLE
// ============================================================

void PendantController::handle(
    const Event& event
)
{
    bool changed =
        false;


    switch(event.type)
    {

        // ----------------------------------------------------
        // AXIS X
        // ----------------------------------------------------

        case EVENT_KEY_8:

            if(
                pendantState.axis !=
                AXIS_X
            )
            {
                pendantState.axis =
                    AXIS_X;

                changed =
                    true;
            }

            break;


        // ----------------------------------------------------
        // AXIS Y
        // ----------------------------------------------------

        case EVENT_KEY_4:

            if(
                pendantState.axis !=
                AXIS_Y
            )
            {
                pendantState.axis =
                    AXIS_Y;

                changed =
                    true;
            }

            break;


        // ----------------------------------------------------
        // AXIS Z
        // ----------------------------------------------------

        case EVENT_KEY_7:

            if(
                pendantState.axis !=
                AXIS_Z
            )
            {
                pendantState.axis =
                    AXIS_Z;

                changed =
                    true;
            }

            break;


        // ----------------------------------------------------
        // JOG STEP GROTER
        // ----------------------------------------------------

        case EVENT_KEY_6:

            if(
                pendantState.jogStep !=
                STEP_10_MM
            )
            {
                pendantState.jogStep =
                    static_cast<JogStep>(
                        pendantState.jogStep - 1
                    );

                changed =
                    true;
            }

            break;


        // ----------------------------------------------------
        // JOG STEP KLEINER
        // ----------------------------------------------------

        case EVENT_KEY_9:

            if(
                pendantState.jogStep !=
                STEP_0_01_MM
            )
            {
                pendantState.jogStep =
                    static_cast<JogStep>(
                        pendantState.jogStep + 1
                    );

                changed =
                    true;
            }

            break;


        // ----------------------------------------------------
        // ENCODER PRESS
        // ----------------------------------------------------

        case EVENT_ENCODER_PRESS:

            toggleLayer();

            break;
        
        case EVENT_ENCODER_PULSE:

            handleJogEncoder(event);

            break;
        // ----------------------------------------------------
        // DEFAULT
        // ----------------------------------------------------

        default:

            break;
    }


    if(changed)
    {
        displayDirty =
            true;
    }
}


void PendantController::toggleLayer()
{
    if(pendantState.layer == LAYER_JOG)
        setLayer(LAYER_INFO);

    else if(pendantState.layer == LAYER_INFO)
        setLayer(LAYER_CONTROL);

    else
        setLayer(LAYER_JOG);
}


void PendantController::setLayer(
    PendantLayer layer
)
{
    if(pendantState.layer == layer)
        return;

    pendantState.layer = layer;

    switch(layer)
    {
        case LAYER_JOG:
            enterJogLayer();
            break;

        case LAYER_INFO:
            break;

        case LAYER_CONTROL:
            break;
    }

    displayDirty =
        true;
}

void PendantController::enterJogLayer()
{
    MachineSettings machineSettings =
        cnc->machineSettingsSnapshot();

    jogPlanner.begin(
        machineSettings
    );

    jogPlanner.setJogStepDistance(
        jogStepDistance()
    );
}


// ============================================================
// AXIS
// ============================================================

Axis PendantController::axis() const
{
    return pendantState.axis;
}



// ============================================================
// JOG STEP DISTANCE
// ============================================================

float PendantController::jogStepDistance() const
{
    return ::jogStepDistance(
        pendantState.jogStep
    );
}

void PendantController::handleJogEncoder(
    const Event& event
)
{
    jogPlanner.setJogStepDistance(
        jogStepDistance()
    );

    jogPlanner.encoder(
        event,
        axis()
    );
}



// ============================================================
// UPDATE
// ============================================================

void PendantController::update()
{
    /*
        Dit gebeurt iedere loop, maar veroorzaakt zelf
        géén display-update.

        We kijken alleen of een externe status veranderd is.
    */

    MachineState machineState =
        cnc->machineStateSnapshot();

    JogCommand jog =
        jogPlanner.update(
            machineState
        );

    if(jog.type != JOG_NONE)
    {
        cnc->execute(
            jog
        );
    }
    

    checkStatusChanges();


    /*
        Alleen daadwerkelijk naar het display schrijven
        wanneer daar aanleiding voor is.
    */

    if(displayDirty)
    {
        updateDisplay();

        displayDirty =
            false;
    }
}



// ============================================================
// CHECK STATUS CHANGES
// ============================================================

void PendantController::checkStatusChanges()
{
    MachineState machineState =
        cnc->machineStateSnapshot();


    CNCjsInterface::CNCjsStatus currentCncStatus =
        cnc->status();


    if(
        currentCncStatus !=
        lastCncStatus
    )
    {
        lastCncStatus =
            currentCncStatus;


        displayDirty =
            true;


        Serial.print(
            "[Pendant] CNCjs status changed: "
        );


        Serial.println(
            cncStatusName()
        );
    }


    MachineStatus currentMachineStatus =
        machineState.machineStatus;


    if(
        currentMachineStatus !=
        lastMachineStatus
    )
    {
        lastMachineStatus =
            currentMachineStatus;


        displayDirty =
            true;


        Serial.print(
            "[Pendant] Machine status changed: "
        );


        Serial.println(
            machineStatusName()
        );
    }
}


// ============================================================
// DISPLAY
// ============================================================

void PendantController::updateDisplay()
{
    MachineState machineState =
    cnc->machineStateSnapshot();

    if(display == nullptr)
    {
        return;
    }


    display->setTitle(
        "Pendant"
    );


    /*
        --------------------------------------------------------
        PRIORITEIT 1 + 2

        ALARM en HOLD winnen altijd.
        --------------------------------------------------------
    */


    if(
        machineState.machineStatus ==
        MACHINE_ALARM
    )
    {
        updateMachineStatus();

        display->update();

        return;
    }


    if(
        machineState.machineStatus ==
        MACHINE_HOLD
    )
    {
        updateMachineStatus();

        display->update();

        return;
    }


    /*
        --------------------------------------------------------
        PRIORITEIT 3 + 4

        CNCjs heeft voorrang op de normale pendantweergave
        zolang de verbinding/controller nog niet klaar is.
        --------------------------------------------------------
    */

    if(cnc != nullptr)
    {
        CNCjsInterface::CNCjsStatus status =
            cnc->status();


        switch(status)
        {
            case CNCjsInterface::CNCjsStatus::Offline:

            case CNCjsInterface::CNCjsStatus::WiFiConnecting:

            case CNCjsInterface::CNCjsStatus::Authenticating:

            case CNCjsInterface::CNCjsStatus::Connecting:

            case CNCjsInterface::CNCjsStatus::WaitingForLists:

            case CNCjsInterface::CNCjsStatus::ControllerSelectionPending:

            case CNCjsInterface::CNCjsStatus::OpeningController:

                updateCncStatus();

                display->update();

                return;


            default:

                break;
        }
    }


    /*
        --------------------------------------------------------
        NORMALE PENDANT
        --------------------------------------------------------
    */

    updateNormalDisplay();

    display->update();
}



// ============================================================
// CNC STATUS DISPLAY
// ============================================================

void PendantController::updateCncStatus()
{
    if(
        display == nullptr ||
        cnc == nullptr
    )
    {
        return;
    }


    CNCjsInterface::CNCjsStatus status =
        cnc->status();


    display->setStatus(
        cncStatusName()
    );


    switch(status)
    {

        case CNCjsInterface::CNCjsStatus::Offline:

            display->setLine1(
                "Offline"
            );

            display->setLine2(
                "Reconnect"
            );

            break;


        case CNCjsInterface::CNCjsStatus::WiFiConnecting:

            display->setLine1(
                "WiFi"
            );

            display->setLine2(
                "Connecting"
            );

            break;


        case CNCjsInterface::CNCjsStatus::Authenticating:

            display->setLine1(
                "CNCjs"
            );

            display->setLine2(
                "Authenticating"
            );

            break;


        case CNCjsInterface::CNCjsStatus::Connecting:

            display->setLine1(
                "CNCjs"
            );

            display->setLine2(
                "Connecting"
            );

            break;


        case CNCjsInterface::CNCjsStatus::WaitingForLists:

            display->setLine1(
                "CNCjs"
            );

            display->setLine2(
                "Waiting..."
            );

            break;


        case CNCjsInterface::CNCjsStatus::ControllerSelectionPending:

            display->setLine1(
                "Controller"
            );

            display->setLine2(
                "Select"
            );

            break;


        case CNCjsInterface::CNCjsStatus::OpeningController:

            display->setLine1(
                "Opening"
            );

            display->setLine2(
                "Controller"
            );

            break;


        case CNCjsInterface::CNCjsStatus::Error:

            display->setLine1(
                "CNCjs"
            );

            display->setLine2(
                "Error"
            );

            break;


        default:

            display->setLine1(
                "CNCjs"
            );

            display->setLine2(
                ""
            );

            break;
    }
}



// ============================================================
// MACHINE STATUS DISPLAY
// ============================================================

void PendantController::updateMachineStatus()
{
    MachineState machineState =
        cnc->machineStateSnapshot();

    display->setStatus(
        machineStatusName()
    );


    char buffer[20];


    snprintf(
        buffer,
        sizeof(buffer),
        "X%.2f Y%.2f",
        machineState.workPosition.x,
        machineState.workPosition.y
    );


    display->setLine1(
        buffer
    );


    snprintf(
        buffer,
        sizeof(buffer),
        "Z%.2f",
        machineState.workPosition.z
    );


    display->setLine2(
        buffer
    );
}



// ============================================================
// NORMAL DISPLAY
// ============================================================

void PendantController::updateNormalDisplay()
{
    display->setStatus(
        layerName()
    );


    switch(pendantState.layer)
    {

        case LAYER_JOG:

            display->setLine1(
                axisName()
            );


            {
                char buffer[20];


                snprintf(
                    buffer,
                    sizeof(buffer),
                    "Step %.2f",
                    jogStepDistance()
                );


                display->setLine2(
                    buffer
                );
            }

            break;


        case LAYER_INFO:

            display->setLine1(
                "WCS G54"
            );

            display->setLine2(
                "Offsets"
            );

            break;


        case LAYER_CONTROL:

            display->setLine1(
                "Machine"
            );

            display->setLine2(
                "Ready"
            );

            break;
    }
}



// ============================================================
// LAYER NAME
// ============================================================

const char*
PendantController::layerName() const
{
    switch(pendantState.layer)
    {

        case LAYER_JOG:
            return "JOG";


        case LAYER_INFO:
            return "INFO";


        case LAYER_CONTROL:
            return "CONTROL";
    }


    return "";
}



// ============================================================
// AXIS NAME
// ============================================================

const char*
PendantController::axisName() const
{
    switch(pendantState.axis)
    {

        case AXIS_X:
            return "Axis X";


        case AXIS_Y:
            return "Axis Y";


        case AXIS_Z:
            return "Axis Z";


        default:
            return "";
    }
}



// ============================================================
// MACHINE STATUS NAME
// ============================================================

const char* PendantController::machineStatusName() const
{
    MachineState machineState =
        cnc->machineStateSnapshot();

    switch(machineState.machineStatus)
    {
        case MACHINE_DISCONNECTED:
            return "Disconnected";

        case MACHINE_IDLE:
            return "Idle";

        case MACHINE_RUN:
            return "Run";

        case MACHINE_HOLD:
            return "Hold";

        case MACHINE_ALARM:
            return "Alarm";

        default:
            return "Unknown";
    }
}


// ============================================================
// CNC STATUS NAME
// ============================================================

const char*
PendantController::cncStatusName() const
{
    if(cnc == nullptr)
    {
        return "CNCjs";
    }


    switch(cnc->status())
    {

        case CNCjsInterface::CNCjsStatus::Offline:
            return "OFFLINE";


        case CNCjsInterface::CNCjsStatus::WiFiConnecting:
            return "CONNECTING";


        case CNCjsInterface::CNCjsStatus::Authenticating:
            return "AUTHENTICATING";


        case CNCjsInterface::CNCjsStatus::Connecting:
            return "CONNECTING";


        case CNCjsInterface::CNCjsStatus::WaitingForLists:
            return "WAITING";


        case CNCjsInterface::CNCjsStatus::ControllerSelectionPending:
            return "SELECT";


        case CNCjsInterface::CNCjsStatus::OpeningController:
            return "CONNECTING";


        case CNCjsInterface::CNCjsStatus::Ready:
            return "READY";


        case CNCjsInterface::CNCjsStatus::Error:
            return "ERROR";
    }


    return "";
}