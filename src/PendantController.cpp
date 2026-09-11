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


    handlers.encoderLongPress =
        &PendantController::toggleLayer;


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


    setLayer(
        LAYER_JOG
    );

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


        // --------------------------------------------------------
        // ENCODER PRESS
        // --------------------------------------------------------

        case EVENT_ENCODER_PRESS:

            if(handlers.encoderPress != nullptr)
                (this->*handlers.encoderPress)();

            break;


        // --------------------------------------------------------
        // ENCODER LONG PRESS
        // --------------------------------------------------------

        case EVENT_ENCODER_LONG_PRESS:

            if(handlers.encoderLongPress != nullptr)
                (this->*handlers.encoderLongPress)();

            break;


        // --------------------------------------------------------
        // ENCODER PULSE
        // --------------------------------------------------------

        case EVENT_ENCODER_PULSE:

            if(handlers.encoderPulse != nullptr)
                (this->*handlers.encoderPulse)(event);

            break;


        // --------------------------------------------------------
        // CHANGED TO READY
        // --------------------------------------------------------

        case EVENT_CHANGED_TO_READY:

            if(handlers.changedToReady != nullptr)
                (this->*handlers.changedToReady)();

            break;


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
    {
        setLayer(
            LAYER_INFO
        );
    }
    else if(pendantState.layer == LAYER_INFO)
    {
        setLayer(
            LAYER_COMMAND
        );
    }
    else
    {
        setLayer(
            LAYER_JOG
        );
    }
}


void PendantController::setLayer(
    PendantLayer layer
)
{
    if(pendantState.layer == layer)
        return;


    /*
        Bij het wisselen van layer mogen geen handlers
        van de vorige layer blijven staan.
    */

    clearHandlers();


    /*
        Een eventuele openstaande Command-confirmatie
        vervalt wanneer we van layer wisselen.
    */

    pendingCommandAction =
        COMMAND_ACTION_NONE;

    commandActionStartedAt =
        0;


    pendantState.layer =
        layer;


    switch(layer)
    {
        case LAYER_JOG:

            enterJogLayer();

            break;


        case LAYER_INFO:

            enterInfoLayer();

            break;


        case LAYER_COMMAND:

            enterCommandLayer();

            break;
    }


    /*
        Long press van de encoder is globaal:
        daarmee wisselen we altijd van layer.
    */

    handlers.encoderLongPress =
        &PendantController::toggleLayer;


    displayDirty =
        true;
}


// ============================================================
// CLEAR HANDLERS
// ============================================================

void PendantController::clearHandlers()
{
    handlers.key1 =
        nullptr;

    handlers.key2 =
        nullptr;

    handlers.key3 =
        nullptr;

    handlers.key4 =
        nullptr;

    handlers.key5 =
        nullptr;

    handlers.key6 =
        nullptr;

    handlers.key7 =
        nullptr;

    handlers.key8 =
        nullptr;

    handlers.key9 =
        nullptr;

    handlers.encoderPress =
        nullptr;

    handlers.encoderLongPress =
        nullptr;

    handlers.encoderPulse =
        nullptr;

    handlers.changedToReady =
        nullptr;
}


// ============================================================
// JOG LAYER
// ============================================================

void PendantController::enterJogLayer()
{
    handlers.changedToReady =
        &PendantController::initialiseJogPlanner;


    handlers.key1 =
        &PendantController::feedHoldCycleStart;

    handlers.key2 =
        &PendantController::homeAxis;

    handlers.key3 =
        &PendantController::reset;

    handlers.key4 =
        &PendantController::selectYAxis;

    handlers.key5 =
        &PendantController::zeroAxis;

    handlers.key6 =
        &PendantController::increaseJogStep;

    handlers.key7 =
        &PendantController::selectZAxis;

    handlers.key8 =
        &PendantController::selectXAxis;

    handlers.key9 =
        &PendantController::decreaseJogStep;


    handlers.encoderPulse =
        &PendantController::handleJogEncoder;


    if(
        cnc.status() ==
        CNCjsInterface::CNCjsStatus::Ready
    )
    {
        initialiseJogPlanner();
    }
}


// ============================================================
// INFO LAYER
// ============================================================

void PendantController::enterInfoLayer()
{
    // Nog geen specifieke input handlers.
}


// ============================================================
// COMMAND LAYER
// ============================================================

void PendantController::enterCommandLayer()
{
    /*
        KEY 1 is direct Feedhold / Cycle Start.
    */

    handlers.key1 =
        &PendantController::feedHoldCycleStart;

    handlers.key2 =
        &PendantController::gcodeStartPause;

    handlers.key3 =
        &PendantController::requestGcodeStop;
    /*
        Homing vraagt eerst om bevestiging.
    */

    handlers.key4 =
        &PendantController::requestHomeY;

    handlers.key5 =
        &PendantController::requestHomeAll;

    handlers.key7 =
        &PendantController::requestHomeZ;

    handlers.key8 =
        &PendantController::requestHomeX;


    /*
        Encoder press bevestigt de geselecteerde
        CommandAction.
    */

    handlers.encoderPress =
        &PendantController::confirmCommandAction;
}


// ============================================================
// INITIALISE JOG PLANNER
// ============================================================

void PendantController::initialiseJogPlanner()
{
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
// FEEDHOLD / RESUME
// ============================================================

void PendantController::feedHoldCycleStart()
{
    MachineState state =
        cnc.machineStateSnapshot();


    if(
        state.machineStatus ==
        MACHINE_HOLD
    )
    {
        cnc.cyclestart();
    }
    else
    {
        cnc.feedHold();
    }
}

void PendantController::gcodeStartPause()
{
    MachineState state =
        cnc.machineStateSnapshot();


    if(
        state.machineStatus ==
        MACHINE_IDLE
    )
    {
        cnc.gcodeStart();
    }else if(
        state.machineStatus ==
        MACHINE_HOLD     
    ){
        cnc.gcodeResume();
    }   
    
    else if (        state.machineStatus ==
        MACHINE_RUN  )
    {
        cnc.gcodePause();
    }
}



// ============================================================
// AXIS SELECTION
// ============================================================

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


// ============================================================
// JOG STEP
// ============================================================

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


// ============================================================
// JOG ACTIONS
// ============================================================

void PendantController::homeAxis()
{
    cnc.homeAxis(
        axis()
    );
}


void PendantController::unlock()
{
    cnc.unlock();
}


void PendantController::reset()
{
    cnc.reset();
}


void PendantController::zeroAxis()
{
    cnc.zeroAxis(
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
// COMMAND ACTION REQUESTS
// ============================================================

void PendantController::requestHomeX()
{
    pendingCommandAction =
        COMMAND_ACTION_HOME_X;

    commandActionStartedAt =
        millis();

    displayDirty =
        true;
}


void PendantController::requestHomeY()
{
    pendingCommandAction =
        COMMAND_ACTION_HOME_Y;

    commandActionStartedAt =
        millis();

    displayDirty =
        true;
}


void PendantController::requestHomeZ()
{
    pendingCommandAction =
        COMMAND_ACTION_HOME_Z;

    commandActionStartedAt =
        millis();

    displayDirty =
        true;
}


void PendantController::requestHomeAll()
{
    pendingCommandAction =
        COMMAND_ACTION_HOME_ALL;

    commandActionStartedAt =
        millis();

    displayDirty =
        true;
}

// ============================================================
// COMMAND ACTION REQUESTS
// ============================================================

void PendantController::requestGcodeStop()
{
     MachineState state =
    cnc.machineStateSnapshot();


    if(
        state.machineStatus !=
        MACHINE_HOLD
    ){exit;}
    pendingCommandAction =
        COMMAND_ACTION_GCODE_STOP;

    commandActionStartedAt =
        millis();

    displayDirty =
        true;
}


// ============================================================
// COMMAND ACTION CONFIRMATION
// ============================================================

void PendantController::confirmCommandAction()
{
    switch(pendingCommandAction)
    {
        case COMMAND_ACTION_HOME_X:

            cnc.homeAxis(
                AXIS_X
            );

            break;


        case COMMAND_ACTION_HOME_Y:

            cnc.homeAxis(
                AXIS_Y
            );

            break;


        case COMMAND_ACTION_HOME_Z:

            cnc.homeAxis(
                AXIS_Z
            );

            break;


        case COMMAND_ACTION_HOME_ALL:

            cnc.homeAll();

            break;


        case COMMAND_ACTION_GCODE_STOP:

            cnc.gcodeStop();

            break;


        case COMMAND_ACTION_NONE:

            return;
    }


    /*
        De actie is uitgevoerd.
        De bevestigingsvraag verdwijnt onmiddellijk.
    */

    pendingCommandAction =
        COMMAND_ACTION_NONE;

    commandActionStartedAt =
        0;

    displayDirty =
        true;
}


// ============================================================
// COMMAND ACTION TIMEOUT
// ============================================================

void PendantController::checkCommandActionTimeout()
{
    if(
        pendingCommandAction ==
        COMMAND_ACTION_NONE
    )
    {
        return;
    }


    /*
        Gebruik een verschil met millis() in plaats van:

            millis() >= start + timeout

        zodat de normale millis()-overflow geen probleem vormt.
    */

    if(
        millis() - commandActionStartedAt >=
        CONTROL_CONFIRM_TIMEOUT_MS
    )
    {
        pendingCommandAction =
            COMMAND_ACTION_NONE;

        commandActionStartedAt =
            0;

        displayDirty =
            true;
    }
}


// ============================================================
// UPDATE
// ============================================================

void PendantController::update()
{
    /*
        CNCjs is onderdeel van de PendantController.

        Daarom wordt de interface hier geüpdatet en niet
        rechtstreeks vanuit main.cpp.
    */

    cnc.update();


    /*
        --------------------------------------------------------
        COMMAND CONFIRMATION TIMEOUT
        --------------------------------------------------------
    */

    checkCommandActionTimeout();


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


    /*
        De JogPlanner draait uitsluitend in de Jog-layer.

        In Command en Info mag de planner dus geen jog
        commands produceren.
    */

    if(
        pendantState.layer ==
        LAYER_JOG
    )
    {
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
        switch(currentCncStatus)
        {
            case CNCjsInterface::CNCjsStatus::Ready:
            {
                Event event;

                event.type =
                    EVENT_CHANGED_TO_READY;


                handle(
                    event
                );

                break;
            }


            default:

                break;
        }


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
        het display dirty, zodat de COMMAND-laag onmiddellijk
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


    /*
        --------------------------------------------------------
        JOB NAME
        --------------------------------------------------------

        gcode:load / gcode:unload wordt via JobSnapshot
        zichtbaar voor de PendantController.
        --------------------------------------------------------
    */

    JobSnapshot jobSnapshot =
        cnc.jobSnapshot();


    String currentJobName =
        jobSnapshot.valid
            ? jobSnapshot.name
            : "";


    if(
        currentJobName !=
        lastJobName
    )
    {
        lastJobName =
            currentJobName;


        displayDirty =
            true;


        Serial.print(
            "[Pendant] Job changed: "
        );


        if(
            lastJobName.length() > 0
        )
        {
            Serial.println(
                lastJobName
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


    /*
        De normale display-layout bepaalt zelf welke tekst
        bovenaan staat. Daarom is "Pendant" hier niet meer
        nodig als vaste titel.
    */

    display.setTitle(
        ""
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
        MACHINE_HOLD &&
        pendantState.layer ==
        LAYER_JOG
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
    switch(pendantState.layer)
    {
        // ----------------------------------------------------
        // JOG
        // ----------------------------------------------------

        case LAYER_JOG:
        {
            display.setTitle(
                layerName()
            );

            display.setStatus(
                ""
            );

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
        }


        // ----------------------------------------------------
        // INFO
        // ----------------------------------------------------

        case LAYER_INFO:
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


            display.setTitle(
                layerName()
            );

            display.setStatus(
                ""
            );


            display.iconRightTextView(
                INFO_ICON,
                wcsBuffer,
                "Offsets"
            );


            break;
        }


        // ----------------------------------------------------
        // COMMAND
        // ----------------------------------------------------

        case LAYER_COMMAND:
        {
            MachineState machineState =
                cnc.machineStateSnapshot();


            /*
                Een openstaande CommandAction heeft prioriteit
                boven de normale Command-weergave.
            */

            if(
                pendingCommandAction !=
                COMMAND_ACTION_NONE
            )
            {
                display.setTitle(
                    commandActionName()
                );

                display.setStatus(
                    ""
                );


                display.iconLeftTextView(
                    COMMAND_ICON,
                    "Press encoder",
                    ""
                );

                break;
            }


            /*
                ------------------------------------------------
                COMMAND NORMAL
                ------------------------------------------------

                Regel 1:
                    HOME | RUN | STOP

                Regel 2:
                    file: 'feedertest'

                Regel 3:
                    controller: Idle, G54
                ------------------------------------------------
            */

            display.setTitle(
                "HOME|RUN|STOP"
            );


            char fileBuffer[21];


            if(
                lastJobName.length() > 0
            )
            {
                snprintf(
                    fileBuffer,
                    sizeof(fileBuffer),
                    "f: %s",
                    lastJobName.c_str()
                );
            }
            else
            {
                snprintf(
                    fileBuffer,
                    sizeof(fileBuffer),
                    "f: -"
                );
            }


            display.setStatus(
                fileBuffer
            );


            char controllerBuffer[21];


            if(
                machineState.activeWcs.length() > 0
            )
            {
                snprintf(
                    controllerBuffer,
                    sizeof(controllerBuffer),
                    "c: %s, %s",
                    machineStatusName(),
                    machineState.activeWcs.c_str()
                );
            }
            else
            {
                snprintf(
                    controllerBuffer,
                    sizeof(controllerBuffer),
                    "c: %s",
                    machineStatusName()
                );
            }


            display.iconLeftTextView(
                COMMAND_ICON,
                controllerBuffer,
                ""
            );


            /*
                iconLeftTextView() schrijft line1 en line2.
                Daarom zetten we de bestandsnaam daarna opnieuw
                op de statusregel en gebruiken we de titel voor
                HOME | RUN | STOP.
            */

            display.setStatus(
                ""
            );

            display.setLine1(
                fileBuffer
            );

            display.setLine2(
                controllerBuffer
            );


            break;
        }
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


        case LAYER_COMMAND:

            return "COMMAND";
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


// ============================================================
// COMMAND ACTION NAME
// ============================================================

const char*
PendantController::commandActionName() const
{
    switch(pendingCommandAction)
    {
        case COMMAND_ACTION_HOME_X:

            return "Home X?";


        case COMMAND_ACTION_HOME_Y:

            return "Home Y?";


        case COMMAND_ACTION_HOME_Z:

            return "Home Z?";


        case COMMAND_ACTION_HOME_ALL:

            return "Home All?";

        case COMMAND_ACTION_GCODE_STOP:

            return "Stop job?";


        case COMMAND_ACTION_NONE:

            return "";
    }


    return "";
}


// ============================================================
// ICON FOR AXIS
// ============================================================

const uint8_t*
PendantController::iconForAxis(
    Axis axis
) const
{
    switch(axis)
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