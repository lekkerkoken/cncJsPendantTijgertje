#include "PendantController.h"

#include <Arduino.h>


// ============================================================
// BEGIN
// ============================================================

void PendantController::begin(
    ButtonMatrix& matrix,
    Encoder& encoder,
    OLED& oled
)
{
    cnc.begin();

    display.begin(
        oled
    );

    input.begin(
        matrix,
        encoder
    );

    displayDirty =
        true;

    lastCncStatus =
        cnc.status();

    MachineState machineState =
        cnc.machineStateSnapshot();

    lastMachineStatus =
        machineState.machineStatus;


    lastWorkPositionX =
        machineState.workPosition.x;

    lastWorkPositionY =
        machineState.workPosition.y;

    lastWorkPositionZ =
        machineState.workPosition.z;


    enterJogLayer();

    updateDisplay();
}


// ============================================================
// HANDLE
// ============================================================

void PendantController::handle(
    const Event& event
)
{
    
    switch(event.type)
    {

        case EVENT_KEY_1:

            if(handlers.key1 != nullptr)
                (this->*handlers.key1)();

            break;

 
        case EVENT_KEY_2:

            if(handlers.key2 != nullptr)
                (this->*handlers.key2)();

            break;


        case EVENT_KEY_3:

            if(handlers.key3 != nullptr)
                (this->*handlers.key3)();

            break;

        case EVENT_KEY_4:

            if(handlers.key4 != nullptr)
                (this->*handlers.key4)();

            break;

        case EVENT_KEY_5:

            if(handlers.key5 != nullptr)
                (this->*handlers.key5)();

            break;

        case EVENT_KEY_6:

            if(handlers.key6 != nullptr)
                (this->*handlers.key6)();

            break;


        case EVENT_KEY_7:

            if(handlers.key7 != nullptr)
                (this->*handlers.key7)();

            break;


        case EVENT_KEY_8:

            if(handlers.key8 != nullptr)
                (this->*handlers.key8)();

            break;


        case EVENT_KEY_9:

            if(handlers.key9 != nullptr)
                (this->*handlers.key9)();

            break;

        // ----------------------------------------------------
        // ENCODER PRESS
        // ----------------------------------------------------

        case EVENT_ENCODER_PRESS:

            if(handlers.encoderPress != nullptr)
                (this->*handlers.encoderPress)();

            break;

        // ----------------------------------------------------
        // ENCODER LONG PRESS
        // ----------------------------------------------------

        case EVENT_ENCODER_LONG_PRESS:

            toggleLayer();

            break;

        // ----------------------------------------------------
        // ENCODER PULSE
        // ----------------------------------------------------

        case EVENT_ENCODER_PULSE:

            handleJogEncoder(
                event
            );

            break;


        // ----------------------------------------------------
        // DEFAULT
        // ----------------------------------------------------

        default:

            break;
    }

}



// ============================================================
// LAYER
// ============================================================

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


    pendantState.layer =
        layer;


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
    handlers.key1 =
        &PendantController::feedHoldCycleStart;

    handlers.key2 =
        &PendantController::homeAxis;

    handlers.key4 =
        &PendantController::selectYAxis;

    handlers.key6 =
        &PendantController::increaseJogStep;

    handlers.key7 =
        &PendantController::selectZAxis;

    handlers.key8 =
        &PendantController::selectXAxis;

    handlers.key9 =
        &PendantController::decreaseJogStep;


    cnc.cacheControllerSettings();

    MachineSettings machineSettings =
        cnc.machineSettingsSnapshot();

    jogPlanner.begin(
        machineSettings
    );

    jogPlanner.setJogStepDistance(
        jogStepDistance()
    );

    jogPlanner.setAxis(
        axis()
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

// ============================================================
// Feedhold / Resume
// ============================================================

void PendantController::feedHoldCycleStart()
{
    MachineState state = cnc.machineStateSnapshot();

    if(state.machineStatus == MACHINE_HOLD)
    {
        cnc.resume();
    }
    else
    {
        cnc.feedHold();
    }
}

void PendantController::selectXAxis()
{
    if(
        pendantState.axis !=
        AXIS_X
    )
    {
        pendantState.axis =
            AXIS_X;

        displayDirty =
            true;
    }
}

void PendantController::selectYAxis()
{
    if(
        pendantState.axis !=
        AXIS_Y
    )
    {
        pendantState.axis =
            AXIS_Y;

        displayDirty =
            true;
    }
}

void PendantController::selectZAxis()
{
    if(
        pendantState.axis !=
        AXIS_Z
    )
    {
        pendantState.axis =
            AXIS_Z;

        displayDirty =
            true;
    }
}

void PendantController::decreaseJogStep()
{
    if(
        pendantState.jogStep !=
        STEP_0_01_MM
    )
    {
        pendantState.jogStep =
            static_cast<JogStep>(
                pendantState.jogStep + 1
            );

        displayDirty =
            true;
    }
}

void PendantController::increaseJogStep()
{
    if(
        pendantState.jogStep !=
        STEP_10_MM
    )
    {
        pendantState.jogStep =
            static_cast<JogStep>(
                pendantState.jogStep - 1
            );

        displayDirty =
            true;
    }
}

void PendantController::homeAxis()
{   
    cnc.homeAxis(
        axis()
    );
}


// ============================================================
// JOG ENCODER
// ============================================================

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
        CNCjs is onderdeel van de PendantController.

        Daarom wordt de interface hier geüpdatet en niet meer
        rechtstreeks vanuit main.cpp.
    */

    cnc.update();


    /*
        --------------------------------------------------------
        INPUT
        --------------------------------------------------------

        InputManager wordt zelfstandig verwerkt door zijn
        FreeRTOS task.

        De controller consumeert hier alle beschikbare
        events uit de event queue.
        --------------------------------------------------------
    */

    while(
        input.available()
    )
    {
        Event event =
            input.read();


        handle(
            event
        );
    }


    /*
        --------------------------------------------------------
        MACHINE STATE
        --------------------------------------------------------

        Vanaf hier zijn de snapshots actueel voor deze loop.
        --------------------------------------------------------
    */

    MachineState machineState =
        cnc.machineStateSnapshot();


    JogCommand jog =
        jogPlanner.update(
            machineState
        );


    if(
        jog.type !=
        JOG_NONE
    )
    {
        cnc.execute(
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
        cnc.machineStateSnapshot();


    CNCjsInterface::CNCjsStatus currentCncStatus =
        cnc.status();


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


    /*
        --------------------------------------------------------
        ACTIVE WCS
        --------------------------------------------------------

        Een wijziging van G54 -> G55 (of andersom) maakt
        het display dirty, zodat de INFO-laag onmiddellijk
        wordt bijgewerkt.
        --------------------------------------------------------
    */

    if(
        machineState.activeWcs !=
        lastActiveWcs
    )
    {
        lastActiveWcs =
            machineState.activeWcs;


        displayDirty =
            true;


        Serial.print(
            "[Pendant] Active WCS changed: "
        );


        if(
            lastActiveWcs.length() > 0
        )
        {
            Serial.println(
                lastActiveWcs
            );
        }
        else
        {
            Serial.println(
                "(none)"
            );
        }
    }
    if(
        machineState.workPosition.x !=
            lastWorkPositionX ||

        machineState.workPosition.y !=
            lastWorkPositionY ||

        machineState.workPosition.z !=
            lastWorkPositionZ
    )
    {
        lastWorkPositionX =
            machineState.workPosition.x;

        lastWorkPositionY =
            machineState.workPosition.y;

        lastWorkPositionZ =
            machineState.workPosition.z;


        displayDirty =
            true;
    }
}



// ============================================================
// DISPLAY
// ============================================================

void PendantController::updateDisplay()
{
    MachineState machineState =
        cnc.machineStateSnapshot();


    display.setTitle(
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

        display.update();

        return;
    }


    if(
        machineState.machineStatus ==
        MACHINE_HOLD
    )
    {
        updateMachineStatus();

        display.update();

        return;
    }


    /*
        --------------------------------------------------------
        PRIORITEIT 3 + 4

        CNCjs heeft voorrang op de normale pendantweergave
        zolang de verbinding/controller nog niet klaar is.
        --------------------------------------------------------
    */

    CNCjsInterface::CNCjsStatus status =
        cnc.status();


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

            display.update();

            return;


        default:

            break;
    }


    /*
        --------------------------------------------------------
        NORMALE PENDANT
        --------------------------------------------------------
    */

    updateNormalDisplay();

    display.update();
}



// ============================================================
// CNC STATUS DISPLAY
// ============================================================

void PendantController::updateCncStatus()
{
    CNCjsInterface::CNCjsStatus status =
        cnc.status();


    display.setStatus(
        cncStatusName()
    );


    switch(status)
    {
        case CNCjsInterface::CNCjsStatus::Offline:

            display.setLine1(
                "Offline"
            );

            display.setLine2(
                "Reconnect"
            );

            break;


        case CNCjsInterface::CNCjsStatus::WiFiConnecting:

            display.setLine1(
                "WiFi"
            );

            display.setLine2(
                "Connecting"
            );

            break;


        case CNCjsInterface::CNCjsStatus::Authenticating:

            display.setLine1(
                "CNCjs"
            );

            display.setLine2(
                "Authenticating"
            );

            break;


        case CNCjsInterface::CNCjsStatus::Connecting:

            display.setLine1(
                "CNCjs"
            );

            display.setLine2(
                "Connecting"
            );

            break;


        case CNCjsInterface::CNCjsStatus::WaitingForLists:

            display.setLine1(
                "CNCjs"
            );

            display.setLine2(
                "Waiting..."
            );

            break;


        case CNCjsInterface::CNCjsStatus::ControllerSelectionPending:

            display.setLine1(
                "Controller"
            );

            display.setLine2(
                "Select"
            );

            break;


        case CNCjsInterface::CNCjsStatus::OpeningController:

            display.setLine1(
                "Opening"
            );

            display.setLine2(
                "Controller"
            );

            break;


        case CNCjsInterface::CNCjsStatus::Error:

            display.setLine1(
                "CNCjs"
            );

            display.setLine2(
                "Error"
            );

            break;


        default:

            display.setLine1(
                "CNCjs"
            );

            display.setLine2(
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
        cnc.machineStateSnapshot();


    display.setStatus(
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


    display.setLine1(
        buffer
    );


    snprintf(
        buffer,
        sizeof(buffer),
        "Z%.2f",
        machineState.workPosition.z
    );


    display.setLine2(
        buffer
    );
}



// ============================================================
// NORMAL DISPLAY
// ============================================================

void PendantController::updateNormalDisplay()
{
    display.setTitle(
        layerName()
    );


    display.setStatus("");


    switch(pendantState.layer)
    {
        case LAYER_JOG:

            display.clear();

            char buffer[20];


            snprintf(
                buffer,
                sizeof(buffer),
                "Step %.2f",
                jogStepDistance()
            );


            display.iconLeftTextView(
                iconForAxis(
                    pendantState.axis
                ),
                axisName(),
                buffer
            );

            break;


        case LAYER_INFO:

            display.clear();


            {
                MachineState machineState =
                    cnc.machineStateSnapshot();


                char wcsBuffer[20];


                if(
                    machineState.activeWcs.length() > 0
                )
                {
                    snprintf(
                        wcsBuffer,
                        sizeof(wcsBuffer),
                        "WCS %s",
                        machineState.activeWcs.c_str()
                    );
                }
                else
                {
                    snprintf(
                        wcsBuffer,
                        sizeof(wcsBuffer),
                        "WCS"
                    );
                }


                display.iconRightTextView(
                    INFO_ICON,
                    wcsBuffer,
                    "Offsets"
                );
            }

            break;


        case LAYER_CONTROL:
                MachineState machineState =
                    cnc.machineStateSnapshot();
            display.clear();

            display.setIcon(
                nullptr,
                ICON_RIGHT
            );


            display.setLine1(
                "Machine"
            );


            display.setLine2(
                machineStatusName()
            );

            break;
    }


    display.update();
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

const char*
PendantController::machineStatusName() const
{
    MachineState machineState =
        cnc.machineStateSnapshot();


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


const uint8_t*
PendantController::iconForAxis(
    Axis axis
) const
{
    switch(
        axis
    )
    {
        case AXIS_X:

            return JOG_ICON_X;


        case AXIS_Y:

            return JOG_ICON_Y;


        case AXIS_Z:

            return JOG_ICON_Z;


        default:

            return nullptr;
    }
}



// ============================================================
// CNC STATUS NAME
// ============================================================

const char*
PendantController::cncStatusName() const
{
    switch(cnc.status())
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