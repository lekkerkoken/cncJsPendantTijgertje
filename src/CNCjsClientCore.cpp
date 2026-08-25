#include "CNCjsClientCore.h"

#include <Preferences.h>


CNCjsClientCore* CNCjsClientCore::instance =
    nullptr;


// ============================================================
// BEGIN
// ============================================================

void CNCjsClientCore::begin(
    MachineState& machineState
)
{
    instance =
        this;

    machineState_ =
        &machineState;


    Serial.println();
    Serial.println("================================");
    Serial.println(" CNCjsClientCore");
    Serial.println("================================");


    socketConnectedState =
        false;

    authenticatedState =
        false;

    controllerReadyState =
        false;

    controllerSelectionReadyState =
        false;

    startupReceivedState =
        false;

    portListReceivedState =
        false;

    controllerListProcessedState =
        false;

    listRequested =
        false;

    socketBeginRequested =
        false;

    numberOfControllers =
        0;

    numberOfPorts =
        0;

    commandDirty_ =
        false;

    heartbeatWaiting =
        false;

    lastHeartbeatTime =
        0;

    lastMachineStateTime =
        0;

    lastCommandSendTime_ =
        0;


    if (
        machineState_ != nullptr
    )
    {
        *machineState_ =
            MachineState();
    }


    networkManager_.begin();


    loadServerSettings();


    enterConnectionState(
        ConnectionState::Start
    );


    networkMutex_ =
        xSemaphoreCreateMutex();


    if (
        networkMutex_ == nullptr
    )
    {
        Serial.println(
            "[CNCjs] ERROR: Failed to create network mutex"
        );

        return;
    }


    BaseType_t result =
        xTaskCreate(
            CNCjsClientCore::networkTaskEntry,
            "CNCjsNetwork",
            NETWORK_TASK_STACK_SIZE,
            this,
            NETWORK_TASK_PRIORITY,
            &networkTaskHandle_
        );


    if (
        result != pdPASS
    )
    {
        networkTaskHandle_ =
            nullptr;

        Serial.println(
            "[CNCjs] ERROR: Failed to create network task"
        );

        return;
    }


    Serial.println(
        "[CNCjs] Network task started"
    );
}


// ============================================================
// UPDATE
// ============================================================

void CNCjsClientCore::update()
{
    /*
        Network processing runs in the dedicated FreeRTOS
        network task.

        This method remains for interface compatibility.
    */
}


// ============================================================
// LOCK
// ============================================================

bool CNCjsClientCore::lock()
{
    if (
        networkMutex_ == nullptr
    )
    {
        return false;
    }


    return
        xSemaphoreTake(
            networkMutex_,
            portMAX_DELAY
        ) == pdTRUE;
}


// ============================================================
// UNLOCK
// ============================================================

void CNCjsClientCore::unlock()
{
    if (
        networkMutex_ != nullptr
    )
    {
        xSemaphoreGive(
            networkMutex_
        );
    }
}


// ============================================================
// NETWORK TASK ENTRY
// ============================================================

void CNCjsClientCore::networkTaskEntry(
    void* parameter
)
{
    CNCjsClientCore* self =
        static_cast<CNCjsClientCore*>(
            parameter
        );


    if (
        self == nullptr
    )
    {
        vTaskDelete(
            nullptr
        );

        return;
    }


    self->networkTask();


    vTaskDelete(
        nullptr
    );
}


// ============================================================
// NETWORK TASK
// ============================================================

void CNCjsClientCore::networkTask()
{
    while (true)
    {
        if (
            lock()
        )
        {
            socketIO.loop();

            updateConnection();

            updateHeartbeat();


            if (
                commandDirty_
            )
            {
                unsigned long now =
                    millis();


                if (
                    now - lastCommandSendTime_ >=
                    COMMAND_SEND_INTERVAL
                )
                {
                    sendPendingCommand();
                }
            }


            unlock();
        }


        vTaskDelay(
            pdMS_TO_TICKS(
                NETWORK_TASK_DELAY_MS
            )
        );
    }
}


// ============================================================
// STATUS
// ============================================================

CNCjsClientCore::CNCjsStatus
CNCjsClientCore::status() const
{
    return currentStatus_;
}


// ============================================================
// CONNECTION STATE
// ============================================================

void CNCjsClientCore::enterConnectionState(
    ConnectionState state
)
{
    connectionState =
        state;

    stateStartedAt =
        millis();


    switch (state)
    {
        case ConnectionState::Start:

            currentStatus_ =
                CNCjsStatus::Offline;

            break;


        case ConnectionState::WiFiConnecting:

            currentStatus_ =
                CNCjsStatus::WiFiConnecting;

            break;


        case ConnectionState::Resolving:

            currentStatus_ =
                CNCjsStatus::Connecting;

            break;


        case ConnectionState::Authenticating:

            currentStatus_ =
                CNCjsStatus::Authenticating;

            break;


        case ConnectionState::SocketConnecting:

            currentStatus_ =
                CNCjsStatus::Connecting;

            break;


        case ConnectionState::WaitingForLists:

            currentStatus_ =
                CNCjsStatus::WaitingForLists;

            break;


        case ConnectionState::OpeningController:

            currentStatus_ =
                CNCjsStatus::OpeningController;

            break;


        case ConnectionState::Ready:

            currentStatus_ =
                CNCjsStatus::Ready;

            break;


        case ConnectionState::Backoff:

            currentStatus_ =
                CNCjsStatus::Offline;

            retryAt =
                millis() +
                RECONNECT_DELAY;

            break;
    }
}


// ============================================================
// CONNECTION UPDATE
// ============================================================

void CNCjsClientCore::updateConnection()
{
    switch (connectionState)
    {
        case ConnectionState::Start:

            if (
                networkManager_.wifiConnected()
            )
            {
                enterConnectionState(
                    ConnectionState::Resolving
                );
            }
            else
            {
                networkManager_.startWiFiConnection();

                enterConnectionState(
                    ConnectionState::WiFiConnecting
                );
            }

            break;


        case ConnectionState::WiFiConnecting:

            if (
                networkManager_.updateWiFiConnection()
            )
            {
                enterConnectionState(
                    ConnectionState::Resolving
                );
            }
            else if (
                millis() - stateStartedAt >=
                WIFI_TIMEOUT
            )
            {
                connectionFailed(
                    "WiFi timeout"
                );
            }

            break;


        case ConnectionState::Resolving:

            if (
                networkManager_.resolve(
                    serverHost_.c_str(),
                    serverPort_,
                    serverIP_
                )
            )
            {
                networkManager_.resetAuthentication();

                enterConnectionState(
                    ConnectionState::Authenticating
                );
            }
            else
            {
                connectionFailed(
                    "CNCjs resolution failed"
                );
            }

            break;


        case ConnectionState::Authenticating:

            if (
                !networkManager_.authenticationStarted()
            )
            {
                networkManager_.startAuthentication(
                    serverIP_,
                    serverPort_,
                    serverHost_.c_str()
                );
            }


            if (
                networkManager_.updateAuthentication(
                    token
                )
            )
            {
                authenticatedState =
                    networkManager_.authenticated();

                enterConnectionState(
                    ConnectionState::SocketConnecting
                );
            }
            else if (
                millis() - stateStartedAt >=
                AUTH_TIMEOUT
            )
            {
                connectionFailed(
                    "Authentication timeout"
                );
            }

            break;


        case ConnectionState::SocketConnecting:

            if (
                !socketBeginRequested
            )
            {
                connectSocket();
            }


            if (
                socketConnectedState
            )
            {
                enterConnectionState(
                    ConnectionState::WaitingForLists
                );
            }
            else if (
                millis() - stateStartedAt >=
                SOCKET_TIMEOUT
            )
            {
                connectionFailed(
                    "Socket.IO timeout"
                );
            }

            break;


        case ConnectionState::WaitingForLists:

            if (
                startupReceivedState &&
                portListReceivedState &&
                !controllerListProcessedState
            )
            {
                loadControllerList();
            }

            break;


        case ConnectionState::OpeningController:

            if (
                controllerReadyState
            )
            {
                enterConnectionState(
                    ConnectionState::Ready
                );


                if (
                    machineState_ != nullptr
                )
                {
                    machineState_->connected =
                        false;

                    machineState_->machineStatus =
                        MACHINE_DISCONNECTED;
                }


                lastMachineStateTime =
                    millis();

                lastHeartbeatTime =
                    millis();

                heartbeatWaiting =
                    false;
            }

            break;


        case ConnectionState::Ready:

            break;


        case ConnectionState::Backoff:

            if (
                millis() >= retryAt
            )
            {
                enterConnectionState(
                    ConnectionState::Start
                );
            }

            break;
    }
}


// ============================================================
// CONNECTION FAILED
// ============================================================

void CNCjsClientCore::connectionFailed(
    const char* reason
)
{
    Serial.print(
        "[CNCjs] Connection failed: "
    );

    Serial.println(
        reason
    );


    socketConnectedState =
        false;

    authenticatedState =
        false;

    controllerReadyState =
        false;

    controllerSelectionReadyState =
        false;

    startupReceivedState =
        false;

    portListReceivedState =
        false;

    controllerListProcessedState =
        false;

    listRequested =
        false;

    socketBeginRequested =
        false;

    commandDirty_ =
        false;

    heartbeatWaiting =
        false;


    if (
        machineState_ != nullptr
    )
    {
        machineState_->connected =
            false;

        machineState_->machineStatus =
            MACHINE_DISCONNECTED;
    }


    enterConnectionState(
        ConnectionState::Backoff
    );
}


// ============================================================
// WIFI STATUS
// ============================================================

bool CNCjsClientCore::wifiConnected() const
{
    return networkManager_.wifiConnected();
}


// ============================================================
// LOAD SERVER SETTINGS
// ============================================================

void CNCjsClientCore::loadServerSettings()
{
    Preferences preferences;


    if (
        !preferences.begin(
            "cncjs",
            false
        )
    )
    {
        Serial.println(
            "[CNCjs] ERROR: Failed to open NVS namespace"
        );

        serverHost_ =
            "cncjs.local";

        serverPort_ =
            8000;

        return;
    }


    if (
        !preferences.isKey(
            "host"
        )
    )
    {
        preferences.putString(
            "host",
            "cncjs.local"
        );

        Serial.println(
            "[CNCjs] Initialized default host"
        );
    }


    if (
        !preferences.isKey(
            "port"
        )
    )
    {
        preferences.putUShort(
            "port",
            8000
        );

        Serial.println(
            "[CNCjs] Initialized default port"
        );
    }


    serverHost_ =
        preferences.getString(
            "host",
            "cncjs.local"
        );

    serverPort_ =
        preferences.getUShort(
            "port",
            8000
        );


    preferences.end();


    Serial.println();
    Serial.println(
        "[CNCjs] Server settings:"
    );

    Serial.print(
        " Host: "
    );

    Serial.println(
        serverHost_
    );

    Serial.print(
        " Port: "
    );

    Serial.println(
        serverPort_
    );
}


// ============================================================
// SAVE SERVER SETTINGS
// ============================================================

void CNCjsClientCore::saveServerSettings(
    const char* host,
    uint16_t port
)
{
    Preferences preferences;


    if (
        !preferences.begin(
            "cncjs",
            false
        )
    )
    {
        Serial.println(
            "[CNCjs] ERROR: Failed to open NVS namespace"
        );

        return;
    }


    preferences.putString(
        "host",
        host
    );

    preferences.putUShort(
        "port",
        port
    );


    preferences.end();


    serverHost_ =
        host;

    serverPort_ =
        port;
}


// ============================================================
// AUTHENTICATED
// ============================================================

bool CNCjsClientCore::authenticated() const
{
    return authenticatedState;
}


// ============================================================
// CONNECT SOCKET
// ============================================================

void CNCjsClientCore::connectSocket()
{
    if (
        socketBeginRequested
    )
    {
        return;
    }


    socketBeginRequested =
        true;


    String path =
        "/socket.io/?token=";

    path +=
        token;

    path +=
        "&EIO=3";


    String host =
        serverIP_.toString();


    Serial.print(
        "[Socket.IO] Connecting to "
    );

    Serial.print(
        host
    );

    Serial.print(
        ":"
    );

    Serial.println(
        serverPort_
    );


    Serial.print(
        "[Socket.IO] Host: "
    );

    Serial.println(
        host
    );


    Serial.print(
        "[Socket.IO] Path: "
    );

    Serial.println(
        path
    );


    socketIO.begin(
        host.c_str(),
        serverPort_,
        path.c_str()
    );


    socketIO.onEvent(
        CNCjsClientCore::socketIOEvent
    );
}


// ============================================================
// SOCKET STATUS
// ============================================================

bool CNCjsClientCore::socketConnected() const
{
    return socketConnectedState;
}


// ============================================================
// SOCKET CALLBACK
// ============================================================

void CNCjsClientCore::socketIOEvent(
    socketIOmessageType_t type,
    uint8_t* payload,
    size_t length
)
{
    if (
        instance == nullptr
    )
    {
        return;
    }


    instance->handleSocketEvent(
        type,
        payload,
        length
    );
}


// ============================================================
// MACHINE STATUS MAPPING
// ============================================================

MachineStatus CNCjsClientCore::machineStatusFromCNCjs(
    const char* activeState
) const
{
    if (
        activeState == nullptr
    )
    {
        return MACHINE_DISCONNECTED;
    }


    if (
        strcmp(
            activeState,
            "Idle"
        ) == 0
    )
    {
        return MACHINE_IDLE;
    }


    if (
        strcmp(
            activeState,
            "Run"
        ) == 0
    )
    {
        return MACHINE_RUN;
    }


    if (
        strcmp(
            activeState,
            "Hold"
        ) == 0
    )
    {
        return MACHINE_HOLD;
    }


    if (
        strcmp(
            activeState,
            "Alarm"
        ) == 0
    )
    {
        return MACHINE_ALARM;
    }


    return MACHINE_DISCONNECTED;
}


// ============================================================
// UPDATE MACHINE STATE
// ============================================================

void CNCjsClientCore::updateMachineState(
    JsonObject status,
    JsonObject parserstate
)
{
    if (
        machineState_ == nullptr
    )
    {
        return;
    }


    const char* activeState =
        status["activeState"];


    machineState_->machineStatus =
        machineStatusFromCNCjs(
            activeState
        );


    machineState_->connected =
        true;


    JsonObject mpos =
        status["mpos"];


    if (
        !mpos.isNull()
    )
    {
        machineState_->machinePosition.x =
            mpos["x"] |
            0.0f;

        machineState_->machinePosition.y =
            mpos["y"] |
            0.0f;

        machineState_->machinePosition.z =
            mpos["z"] |
            0.0f;
    }


    JsonObject wpos =
        status["wpos"];


    if (
        !wpos.isNull()
    )
    {
        machineState_->workPosition.x =
            wpos["x"] |
            0.0f;

        machineState_->workPosition.y =
            wpos["y"] |
            0.0f;

        machineState_->workPosition.z =
            wpos["z"] |
            0.0f;
    }


    machineState_->feedrate =
        status["feedrate"] |
        0.0f;


    machineState_->spindleSpeed =
        status["spindle"] |
        0;


    machineHeartbeatReceived();
}


// ============================================================
// MACHINE HEARTBEAT RECEIVED
// ============================================================

void CNCjsClientCore::machineHeartbeatReceived()
{
    lastMachineStateTime =
        millis();

    heartbeatWaiting =
        false;
}


// ============================================================
// HEARTBEAT UPDATE
// ============================================================

void CNCjsClientCore::updateHeartbeat()
{
    if (
        connectionState !=
        ConnectionState::Ready
    )
    {
        return;
    }


    unsigned long now =
        millis();


    if (
        now - lastHeartbeatTime >=
        HEARTBEAT_INTERVAL
    )
    {
        if (
            sendStatusReport()
        )
        {
            lastHeartbeatTime =
                now;

            heartbeatWaiting =
                true;
        }
    }


    if (
        now - lastMachineStateTime >=
        HEARTBEAT_TIMEOUT
    )
    {
        if (
            machineState_ != nullptr
        )
        {
            machineState_->connected =
                false;

            machineState_->machineStatus =
                MACHINE_DISCONNECTED;
        }
    }
}


// ============================================================
// STATUS REPORT
// ============================================================

bool CNCjsClientCore::sendStatusReport()
{
    if (
        !socketConnectedState ||
        !controllerReadyState
    )
    {
        return false;
    }


    if (
        activeControllerPortState.length() == 0
    )
    {
        return false;
    }


    JsonDocument doc;


    JsonArray array =
        doc.to<JsonArray>();


    array.add(
        "command"
    );

    array.add(
        activeControllerPortState
    );

    array.add(
        "statusreport"
    );


    String output;


    serializeJson(
        doc,
        output
    );


    Serial.print(
        "[CNCjs] HEARTBEAT: "
    );

    Serial.println(
        output
    );


    return socketIO.sendEVENT(
        output
    );
}


// ============================================================
// SOCKET EVENT HANDLER
// ============================================================

void CNCjsClientCore::handleSocketEvent(
    socketIOmessageType_t type,
    uint8_t* payload,
    size_t length
)
{
    switch (type)
    {
        case sIOtype_DISCONNECT:
        {
            if (
                !socketConnectedState &&
                connectionState ==
                    ConnectionState::SocketConnecting
            )
            {
                Serial.println(
                    "[Socket.IO] Initial disconnect ignored"
                );

                break;
            }


            socketConnectedState =
                false;

            controllerReadyState =
                false;

            startupReceivedState =
                false;

            portListReceivedState =
                false;

            controllerListProcessedState =
                false;

            listRequested =
                false;

            socketBeginRequested =
                false;

            commandDirty_ =
                false;

            heartbeatWaiting =
                false;


            currentStatus_ =
                CNCjsStatus::Offline;


            if (
                machineState_ != nullptr
            )
            {
                machineState_->connected =
                    false;

                machineState_->machineStatus =
                    MACHINE_DISCONNECTED;
            }


            Serial.println(
                "[Socket.IO] Disconnected"
            );


            enterConnectionState(
                ConnectionState::Backoff
            );


            break;
        }


        case sIOtype_CONNECT:
        {
            socketConnectedState =
                true;

            controllerReadyState =
                false;

            startupReceivedState =
                false;

            portListReceivedState =
                false;

            controllerListProcessedState =
                false;

            listRequested =
                false;


            currentStatus_ =
                CNCjsStatus::WaitingForLists;


            Serial.print(
                "[Socket.IO] Connected to: "
            );

            Serial.println(
                (char*)payload
            );


            requestPortList();


            break;
        }


        case sIOtype_EVENT:
        {
            Serial.print(
                "[IOc] EVENT: "
            );

            Serial.write(
                payload,
                length
            );

            Serial.println();


            JsonDocument doc;


            DeserializationError error =
                deserializeJson(
                    doc,
                    payload,
                    length
                );


            if (
                error
            )
            {
                Serial.print(
                    "[CNCjs] Event JSON error: "
                );

                Serial.println(
                    error.c_str()
                );

                break;
            }


            JsonArray array =
                doc.as<JsonArray>();


            if (
                array.isNull()
            )
            {
                break;
            }


            const char* eventName =
                array[0];


            if (
                eventName == nullptr
            )
            {
                break;
            }


            // ------------------------------------------------
            // STARTUP
            // ------------------------------------------------

            if (
                strcmp(
                    eventName,
                    "startup"
                ) == 0
            )
            {
                JsonArray controllerArray =
                    array[1]["loadedControllers"];


                numberOfControllers =
                    0;


                for (
                    JsonVariant value :
                    controllerArray
                )
                {
                    if (
                        numberOfControllers >=
                        MAX_CONTROLLERS
                    )
                    {
                        break;
                    }


                    const char* name =
                        value;


                    if (
                        name != nullptr
                    )
                    {
                        controllers[
                            numberOfControllers
                        ] =
                            name;

                        numberOfControllers++;
                    }
                }


                startupReceivedState =
                    true;


                Serial.println();

                Serial.print(
                    "[CNCjs] Controllers found: "
                );

                Serial.println(
                    numberOfControllers
                );


                for (
                    int i = 0;
                    i < numberOfControllers;
                    i++
                )
                {
                    Serial.print(
                        "  "
                    );

                    Serial.print(
                        i
                    );

                    Serial.print(
                        ": "
                    );

                    Serial.println(
                        controllers[i]
                    );
                }


                if (
                    startupReceivedState &&
                    portListReceivedState &&
                    !controllerListProcessedState
                )
                {
                    loadControllerList();
                }


                break;
            }


            // ------------------------------------------------
            // SERIAL PORT LIST
            // ------------------------------------------------

            if (
                strcmp(
                    eventName,
                    "serialport:list"
                ) == 0
            )
            {
                JsonArray portArray =
                    array[1];


                numberOfPorts =
                    0;


                for (
                    JsonObject portObject :
                    portArray
                )
                {
                    if (
                        numberOfPorts >=
                        MAX_PORTS
                    )
                    {
                        break;
                    }


                    const char* portName =
                        portObject["port"];


                    if (
                        portName != nullptr
                    )
                    {
                        ports[
                            numberOfPorts
                        ] =
                            portName;

                        numberOfPorts++;
                    }
                }


                portListReceivedState =
                    true;


                Serial.print(
                    "[CNCjs] Ports found: "
                );

                Serial.println(
                    numberOfPorts
                );


                for (
                    int i = 0;
                    i < numberOfPorts;
                    i++
                )
                {
                    Serial.print(
                        "  "
                    );

                    Serial.print(
                        i
                    );

                    Serial.print(
                        ": "
                    );

                    Serial.println(
                        ports[i]
                    );
                }


                if (
                    startupReceivedState &&
                    portListReceivedState &&
                    !controllerListProcessedState
                )
                {
                    loadControllerList();
                }


                break;
            }


            // ------------------------------------------------
            // SERIAL PORT OPEN
            // ------------------------------------------------

            if (
                strcmp(
                    eventName,
                    "serialport:open"
                ) == 0
            )
            {
                JsonObject info =
                    array[1];


                const char* portName =
                    info["port"];


                const char* controllerType =
                    info["controllerType"];


                int baudrate =
                    info["baudrate"];


                if (
                    portName != nullptr &&
                    controllerType != nullptr
                )
                {
                    activeControllerPortState =
                        portName;

                    activeControllerTypeState =
                        controllerType;

                    activeControllerBaudrateState =
                        baudrate;


                    controllerReadyState =
                        false;


                    enterConnectionState(
                        ConnectionState::OpeningController
                    );


                    if (
                        machineState_ != nullptr
                    )
                    {
                        machineState_->connected =
                            false;

                        machineState_->machineStatus =
                            MACHINE_DISCONNECTED;
                    }


                    Serial.println();

                    Serial.println(
                        "[CNCjs] Serial port opened"
                    );


                    Serial.print(
                        "  Port: "
                    );

                    Serial.println(
                        activeControllerPortState
                    );


                    Serial.print(
                        "  Controller: "
                    );

                    Serial.println(
                        activeControllerTypeState
                    );


                    Serial.print(
                        "  Baudrate: "
                    );

                    Serial.println(
                        activeControllerBaudrateState
                    );
                }


                break;
            }


            // ------------------------------------------------
            // CONTROLLER SETTINGS
            // ------------------------------------------------

            if (
                strcmp(
                    eventName,
                    "controller:settings"
                ) == 0
            )
            {
                Serial.println(
                    "[CNCjs] Controller settings received"
                );

                break;
            }


            // ------------------------------------------------
            // CONTROLLER STATE
            // ------------------------------------------------

            if (
                strcmp(
                    eventName,
                    "controller:state"
                ) == 0
            )
            {
                Serial.println(
                    "[CNCjs] Controller state received"
                );


                controllerReadyState =
                    true;


                enterConnectionState(
                    ConnectionState::Ready
                );


                Serial.println();
                Serial.println(
                    "[CNCjs] Controller READY"
                );


                break;
            }


            // ------------------------------------------------
            // GRBL STATE
            // ------------------------------------------------

            if (
                strcmp(
                    eventName,
                    "Grbl:state"
                ) == 0
            )
            {
                JsonObject status =
                    array[1]["status"];


                JsonObject parserstate =
                    array[1]["parserstate"];


                updateMachineState(
                    status,
                    parserstate
                );


                break;
            }


            // ------------------------------------------------
            // SERIAL PORT READ
            // ------------------------------------------------

            if (
                strcmp(
                    eventName,
                    "serialport:read"
                ) == 0
            )
            {
                const char* response =
                    array[1];


                if (
                    response == nullptr
                )
                {
                    break;
                }


                if (
                    response[0] != '<'
                )
                {
                    break;
                }


                if (
                    machineState_ != nullptr
                )
                {
                    machineHeartbeatReceived();

                    machineState_->connected =
                        true;
                }


                const char* stateStart =
                    response + 1;


                const char* stateEnd =
                    strchr(
                        stateStart,
                        '|'
                    );


                if (
                    stateEnd != nullptr &&
                    machineState_ != nullptr
                )
                {
                    size_t stateLength =
                        stateEnd -
                        stateStart;


                    if (
                        stateLength < 20
                    )
                    {
                        char activeState[20];


                        memcpy(
                            activeState,
                            stateStart,
                            stateLength
                        );


                        activeState[
                            stateLength
                        ] =
                            '\0';


                        machineState_->machineStatus =
                            machineStatusFromCNCjs(
                                activeState
                            );
                    }
                }


                const char* mposStart =
                    strstr(
                        response,
                        "MPos:"
                    );


                if (
                    mposStart != nullptr &&
                    machineState_ != nullptr
                )
                {
                    mposStart +=
                        5;


                    float x;
                    float y;
                    float z;


                    if (
                        sscanf(
                            mposStart,
                            "%f,%f,%f",
                            &x,
                            &y,
                            &z
                        ) == 3
                    )
                    {
                        machineState_->machinePosition.x =
                            x;

                        machineState_->machinePosition.y =
                            y;

                        machineState_->machinePosition.z =
                            z;
                    }
                }


                const char* fsStart =
                    strstr(
                        response,
                        "FS:"
                    );


                if (
                    fsStart != nullptr &&
                    machineState_ != nullptr
                )
                {
                    fsStart +=
                        3;


                    float feedrate;


                    if (
                        sscanf(
                            fsStart,
                            "%f",
                            &feedrate
                        ) == 1
                    )
                    {
                        machineState_->feedrate =
                            feedrate;
                    }
                }


                break;
            }


            break;
        }


        case sIOtype_ACK:

            Serial.print(
                "[IOc] ACK: "
            );

            Serial.write(
                payload,
                length
            );

            Serial.println();

            break;


        case sIOtype_ERROR:

            Serial.print(
                "[IOc] ERROR: "
            );

            Serial.write(
                payload,
                length
            );

            Serial.println();

            break;


        default:

            break;
    }
}


// ============================================================
// REQUEST PORT LIST
// ============================================================

void CNCjsClientCore::requestPortList()
{
    if (
        listRequested
    )
    {
        return;
    }


    JsonDocument doc;


    JsonArray array =
        doc.to<JsonArray>();


    array.add(
        "list"
    );

    array.add(
        nullptr
    );


    String output;


    serializeJson(
        doc,
        output
    );


    Serial.print(
        "[CNCjs] TX: "
    );

    Serial.println(
        output
    );


    socketIO.sendEVENT(
        output
    );


    listRequested =
        true;
}


// ============================================================
// CONTROLLER SELECTION READY
// ============================================================

bool CNCjsClientCore::controllerSelectionReady() const
{
    return controllerSelectionReadyState;
}


// ============================================================
// CONTROLLER COUNT
// ============================================================

int CNCjsClientCore::controllerCount() const
{
    return numberOfControllers;
}


// ============================================================
// CONTROLLER
// ============================================================

const char*
CNCjsClientCore::controller(
    int index
) const
{
    if (
        index < 0 ||
        index >= numberOfControllers
    )
    {
        return nullptr;
    }


    return controllers[index].c_str();
}


// ============================================================
// PORT COUNT
// ============================================================

int CNCjsClientCore::portCount() const
{
    return numberOfPorts;
}


// ============================================================
// PORT
// ============================================================

const char*
CNCjsClientCore::port(
    int index
) const
{
    if (
        index < 0 ||
        index >= numberOfPorts
    )
    {
        return nullptr;
    }


    return ports[index].c_str();
}


// ============================================================
// LOAD SAVED CONTROLLER
// ============================================================

int CNCjsClientCore::loadSavedController()
{
    Preferences preferences;


    if (
        !preferences.begin(
            "cncjs",
            true
        )
    )
    {
        return -1;
    }


    int index =
        -1;


    if (
        preferences.isKey(
            "controllerIndex"
        )
    )
    {
        index =
            preferences.getInt(
                "controllerIndex",
                -1
            );
    }


    preferences.end();


    return index;
}


// ============================================================
// LOAD SAVED CONTROLLER NAME
// ============================================================

String CNCjsClientCore::loadSavedControllerName()
{
    Preferences preferences;


    if (
        !preferences.begin(
            "cncjs",
            true
        )
    )
    {
        return "";
    }


    String name;


    if (
        preferences.isKey(
            "controllerName"
        )
    )
    {
        name =
            preferences.getString(
                "controllerName",
                ""
            );
    }


    preferences.end();


    return name;
}


// ============================================================
// LOAD SAVED PORT NAME
// ============================================================

String CNCjsClientCore::loadSavedPortName()
{
    Preferences preferences;


    if (
        !preferences.begin(
            "cncjs",
            true
        )
    )
    {
        return "";
    }


    String name;


    if (
        preferences.isKey(
            "portName"
        )
    )
    {
        name =
            preferences.getString(
                "portName",
                ""
            );
    }


    preferences.end();


    return name;
}


// ============================================================
// SAVE SELECTED CONTROLLER
// ============================================================

void CNCjsClientCore::saveSelectedController(
    int index,
    const char* name
)
{
    Preferences preferences;


    if (
        !preferences.begin(
            "cncjs",
            false
        )
    )
    {
        Serial.println(
            "[CNCjs] ERROR: Failed to open NVS for controller"
        );

        return;
    }


    preferences.putInt(
        "controllerIndex",
        index
    );


    preferences.putString(
        "controllerName",
        name
    );


    preferences.end();


    Serial.print(
        "[CNCjs] Saved controller: "
    );

    Serial.println(
        name
    );
}


// ============================================================
// SAVE SELECTED PORT
// ============================================================

void CNCjsClientCore::saveSelectedPort(
    const char* portName
)
{
    Preferences preferences;


    if (
        !preferences.begin(
            "cncjs",
            false
        )
    )
    {
        Serial.println(
            "[CNCjs] ERROR: Failed to open NVS for port"
        );

        return;
    }


    preferences.putString(
        "portName",
        portName
    );


    preferences.end();


    Serial.print(
        "[CNCjs] Saved port: "
    );

    Serial.println(
        portName
    );
}


// ============================================================
// FIND CONTROLLER BY NAME
// ============================================================

int CNCjsClientCore::findControllerByName(
    const char* name
) const
{
    if (
        name == nullptr ||
        name[0] == '\0'
    )
    {
        return -1;
    }


    for (
        int i = 0;
        i < numberOfControllers;
        i++
    )
    {
        if (
            controllers[i].equals(
                name
            )
        )
        {
            return i;
        }
    }


    return -1;
}


// ============================================================
// FIND PORT BY NAME
// ============================================================

int CNCjsClientCore::findPortByName(
    const char* name
) const
{
    if (
        name == nullptr ||
        name[0] == '\0'
    )
    {
        return -1;
    }


    for (
        int i = 0;
        i < numberOfPorts;
        i++
    )
    {
        if (
            ports[i].equals(
                name
            )
        )
        {
            return i;
        }
    }


    return -1;
}


// ============================================================
// BAUDRATE
// ============================================================

int CNCjsClientCore::baudrateForController(
    const char* controllerType
) const
{
    if (
        controllerType == nullptr
    )
    {
        return 115200;
    }


    if (
        strcmp(
            controllerType,
            "Grbl"
        ) == 0
    )
    {
        return 115200;
    }


    if (
        strcmp(
            controllerType,
            "Marlin"
        ) == 0
    )
    {
        return 115200;
    }


    if (
        strcmp(
            controllerType,
            "Smoothie"
        ) == 0
    )
    {
        return 115200;
    }


    if (
        strcmp(
            controllerType,
            "TinyG"
        ) == 0
    )
    {
        return 115200;
    }


    return 115200;
}


// ============================================================
// LOAD CONTROLLER LIST / SELECTION
// ============================================================

void CNCjsClientCore::loadControllerList()
{
    if (
        !startupReceivedState ||
        !portListReceivedState
    )
    {
        return;
    }


    if (
        controllerListProcessedState
    )
    {
        return;
    }


    controllerListProcessedState =
        true;


    String savedControllerName =
        loadSavedControllerName();


    String savedPortName =
        loadSavedPortName();


    int controllerIndex =
        findControllerByName(
            savedControllerName.c_str()
        );


    int portIndex =
        findPortByName(
            savedPortName.c_str()
        );


    /*
        A saved selection is valid only when BOTH entries
        still exist in the lists supplied by CNCjs.
    */

    if (
        controllerIndex >= 0 &&
        portIndex >= 0
    )
    {
        selectedControllerIndex =
            controllerIndex;


        selectedControllerNameState =
            controllers[
                controllerIndex
            ];


        selectedPortIndex =
            portIndex;


        selectedPortNameState =
            ports[
                portIndex
            ];


        controllerSelectionReadyState =
            false;


        Serial.println();
        Serial.println(
            "[CNCjs] Restored saved controller selection:"
        );


        Serial.print(
            "  Controller: "
        );

        Serial.println(
            selectedControllerNameState
        );


        Serial.print(
            "  Port: "
        );

        Serial.println(
            selectedPortNameState
        );


        openSelectedControllerInternal();

        return;
    }


    Serial.println();
    Serial.println(
        "[CNCjs] Controller selection pending."
    );


    if (
        savedControllerName.length() > 0
    )
    {
        Serial.print(
            "  Saved controller not available: "
        );

        Serial.println(
            savedControllerName
        );
    }


    if (
        savedPortName.length() > 0
    )
    {
        Serial.print(
            "  Saved port not available: "
        );

        Serial.println(
            savedPortName
        );
    }


    controllerSelectionReadyState =
        true;


    currentStatus_ =
        CNCjsStatus::ControllerSelectionPending;


    connectionState =
        ConnectionState::WaitingForLists;
}


// ============================================================
// CHOOSE CONTROLLER
// ============================================================

void CNCjsClientCore::chooseController()
{
    if (!lock())
    {
        return;
    }


    if (
        numberOfControllers == 0 ||
        numberOfPorts == 0
    )
    {
        unlock();

        return;
    }


    selectedControllerIndex =
        0;


    selectedControllerNameState =
        controllers[0];


    selectedPortIndex =
        0;


    selectedPortNameState =
        ports[0];


    saveSelectedController(
        selectedControllerIndex,
        selectedControllerNameState.c_str()
    );


    saveSelectedPort(
        selectedPortNameState.c_str()
    );


    controllerSelectionReadyState =
        false;


    openSelectedControllerInternal();


    unlock();
}


// ============================================================
// SELECT CONTROLLER
// ============================================================

bool CNCjsClientCore::selectController(
    int portIndex,
    int controllerIndex
)
{
    if (!lock())
    {
        return false;
    }


    if (
        portIndex < 0 ||
        portIndex >= numberOfPorts
    )
    {
        unlock();

        return false;
    }


    if (
        controllerIndex < 0 ||
        controllerIndex >= numberOfControllers
    )
    {
        unlock();

        return false;
    }


    selectedPortIndex =
        portIndex;


    selectedPortNameState =
        ports[portIndex];


    selectedControllerIndex =
        controllerIndex;


    selectedControllerNameState =
        controllers[
            controllerIndex
        ];


    saveSelectedPort(
        selectedPortNameState.c_str()
    );


    saveSelectedController(
        selectedControllerIndex,
        selectedControllerNameState.c_str()
    );


    controllerSelectionReadyState =
        false;


    bool result =
        openSelectedControllerInternal();


    unlock();


    return result;
}


// ============================================================
// SELECTED CONTROLLER
// ============================================================

int CNCjsClientCore::selectedController() const
{
    return selectedControllerIndex;
}


// ============================================================
// SELECTED CONTROLLER NAME
// ============================================================

const char*
CNCjsClientCore::selectedControllerName() const
{
    return selectedControllerNameState.c_str();
}


// ============================================================
// SELECTED PORT
// ============================================================

int CNCjsClientCore::selectedPort() const
{
    return selectedPortIndex;
}


// ============================================================
// SELECTED PORT NAME
// ============================================================

const char*
CNCjsClientCore::selectedPortName() const
{
    return selectedPortNameState.c_str();
}


// ============================================================
// CONTROLLER READY
// ============================================================

bool CNCjsClientCore::controllerReady() const
{
    return controllerReadyState;
}


// ============================================================
// ACTIVE CONTROLLER PORT
// ============================================================

const char*
CNCjsClientCore::controllerPort() const
{
    return activeControllerPortState.c_str();
}


// ============================================================
// ACTIVE CONTROLLER TYPE
// ============================================================

const char*
CNCjsClientCore::controllerType() const
{
    return activeControllerTypeState.c_str();
}


// ============================================================
// ACTIVE CONTROLLER BAUDRATE
// ============================================================

int CNCjsClientCore::controllerBaudrate() const
{
    return activeControllerBaudrateState;
}


// ============================================================
// OPEN SELECTED CONTROLLER
// ============================================================

bool CNCjsClientCore::openSelectedController()
{
    if (!lock())
    {
        return false;
    }


    bool result =
        openSelectedControllerInternal();


    unlock();


    return result;
}


// ============================================================
// OPEN SELECTED CONTROLLER INTERNAL
// ============================================================

bool CNCjsClientCore::openSelectedControllerInternal()
{
    if (
        selectedPortIndex < 0 ||
        selectedControllerIndex < 0
    )
    {
        return false;
    }


    int baudrate =
        baudrateForController(
            selectedControllerNameState.c_str()
        );


    return openControllerInternal(
        selectedPortNameState.c_str(),
        selectedControllerNameState.c_str(),
        baudrate
    );
}


// ============================================================
// EXECUTE MACHINE COMMAND
// ============================================================

bool CNCjsClientCore::execute(
    const MachineCommand& command
)
{
    if (!lock())
    {
        return false;
    }


    bool result =
        false;


    if (
        connectionState !=
        ConnectionState::Ready
    )
    {
        Serial.println(
            "[CNCjs] execute rejected: not ready"
        );
    }
    else
    {
        switch (command.type)
        {
            case MACHINE_COMMAND_GCODE:

                pendingCommand_ =
                    command;

                commandDirty_ =
                    true;

                result =
                    true;

                break;


            case MACHINE_COMMAND_JOG_CANCEL:

                result =
                    jogCancelInternal();

                break;


            case MACHINE_COMMAND_FEED_HOLD:

                result =
                    feedHoldInternal();

                break;


            case MACHINE_COMMAND_RESUME:

                result =
                    resumeInternal();

                break;


            case MACHINE_COMMAND_RESET:

                result =
                    resetInternal();

                break;


            case MACHINE_COMMAND_NONE:

            default:

                result =
                    false;

                break;
        }
    }


    unlock();


    return result;
}


// ============================================================
// SEND PENDING COMMAND
// ============================================================

bool CNCjsClientCore::sendPendingCommand()
{
    if (
        !commandDirty_
    )
    {
        return false;
    }


    if (
        !socketConnectedState ||
        !controllerReadyState
    )
    {
        return false;
    }


    if (
        pendingCommand_.type !=
        MACHINE_COMMAND_GCODE
    )
    {
        commandDirty_ =
            false;

        return false;
    }


    if (
        activeControllerPortState.length() == 0
    )
    {
        return false;
    }


    JsonDocument doc;


    JsonArray array =
        doc.to<JsonArray>();


    array.add(
        "command"
    );

    array.add(
        activeControllerPortState
    );

    array.add(
        "gcode"
    );

    array.add(
        pendingCommand_.command
    );


    String output;


    serializeJson(
        doc,
        output
    );


    Serial.print(
        "[CNCjs] TX @20Hz: "
    );

    Serial.println(
        output
    );


    bool sent =
        socketIO.sendEVENT(
            output
        );


    if (
        sent
    )
    {
        commandDirty_ =
            false;

        lastCommandSendTime_ =
            millis();
    }


    return sent;
}


// ============================================================
// JOG CANCEL
// ============================================================

bool CNCjsClientCore::jogCancel()
{
    if (!lock())
    {
        return false;
    }


    bool result =
        jogCancelInternal();


    unlock();


    return result;
}


// ============================================================
// JOG CANCEL INTERNAL
// ============================================================

bool CNCjsClientCore::jogCancelInternal()
{
    if (
        !socketConnectedState ||
        !controllerReadyState
    )
    {
        return false;
    }


    JsonDocument doc;


    JsonArray array =
        doc.to<JsonArray>();


    array.add(
        "jogCancel"
    );

    array.add(
        activeControllerPortState
    );


    String output;


    serializeJson(
        doc,
        output
    );


    Serial.print(
        "[CNCjs] JOG CANCEL: "
    );

    Serial.println(
        output
    );


    return socketIO.sendEVENT(
        output
    );
}


// ============================================================
// FEED HOLD
// ============================================================

bool CNCjsClientCore::feedHold()
{
    if (!lock())
    {
        return false;
    }


    bool result =
        feedHoldInternal();


    unlock();


    return result;
}


// ============================================================
// FEED HOLD INTERNAL
// ============================================================

bool CNCjsClientCore::feedHoldInternal()
{
    return sendRealtimeInternal(
        '!'
    );
}


// ============================================================
// RESUME
// ============================================================

bool CNCjsClientCore::resume()
{
    if (!lock())
    {
        return false;
    }


    bool result =
        resumeInternal();


    unlock();


    return result;
}


// ============================================================
// RESUME INTERNAL
// ============================================================

bool CNCjsClientCore::resumeInternal()
{
    return sendRealtimeInternal(
        '~'
    );
}


// ============================================================
// RESET
// ============================================================

bool CNCjsClientCore::reset()
{
    if (!lock())
    {
        return false;
    }


    bool result =
        resetInternal();


    unlock();


    return result;
}


// ============================================================
// RESET INTERNAL
// ============================================================

bool CNCjsClientCore::resetInternal()
{
    Serial.println(
        "[CNCjs] Reset requested"
    );


    return sendRealtimeInternal(
        0x18
    );
}


// ============================================================
// OPEN CONTROLLER
// ============================================================

bool CNCjsClientCore::openController(
    const char* port,
    const char* controllerType,
    int baudrate
)
{
    if (!lock())
    {
        return false;
    }


    bool result =
        openControllerInternal(
            port,
            controllerType,
            baudrate
        );


    unlock();


    return result;
}


// ============================================================
// OPEN CONTROLLER INTERNAL
// ============================================================

bool CNCjsClientCore::openControllerInternal(
    const char* portName,
    const char* controllerType,
    int baudrate
)
{
    if (
        !socketConnectedState
    )
    {
        return false;
    }


    if (
        portName == nullptr ||
        controllerType == nullptr
    )
    {
        return false;
    }


    controllerReadyState =
        false;


    enterConnectionState(
        ConnectionState::OpeningController
    );


    if (
        machineState_ != nullptr
    )
    {
        machineState_->connected =
            false;

        machineState_->machineStatus =
            MACHINE_DISCONNECTED;
    }


    JsonDocument doc;


    JsonArray array =
        doc.to<JsonArray>();


    array.add(
        "open"
    );

    array.add(
        portName
    );


    JsonObject options =
        array.add<JsonObject>();


    options["controllerType"] =
        controllerType;

    options["baudrate"] =
        baudrate;

    options["rtscts"] =
        false;


    JsonObject pin =
        options["pin"].to<JsonObject>();


    pin["dtr"] =
        nullptr;

    pin["rts"] =
        nullptr;


    String output;


    serializeJson(
        doc,
        output
    );


    Serial.print(
        "[CNCjs] OPEN: "
    );

    Serial.println(
        output
    );


    return socketIO.sendEVENT(
        output
    );
}


// ============================================================
// SEND GCODE
// ============================================================

bool CNCjsClientCore::sendGcode(
    const char* gcode
)
{
    if (!lock())
    {
        return false;
    }


    bool result =
        false;


    if (
        controllerReadyState
    )
    {
        result =
            sendGcodeInternal(
                activeControllerPortState.c_str(),
                gcode
            );
    }


    unlock();


    return result;
}


// ============================================================
// SEND GCODE - EXPLICIT PORT
// ============================================================

bool CNCjsClientCore::sendGcode(
    const char* port,
    const char* gcode
)
{
    if (!lock())
    {
        return false;
    }


    bool result =
        sendGcodeInternal(
            port,
            gcode
        );


    unlock();


    return result;
}


// ============================================================
// SEND GCODE INTERNAL
// ============================================================

bool CNCjsClientCore::sendGcodeInternal(
    const char* portName,
    const char* gcode
)
{
    if (
        !socketConnectedState ||
        !controllerReadyState
    )
    {
        return false;
    }


    if (
        portName == nullptr ||
        gcode == nullptr
    )
    {
        return false;
    }


    JsonDocument doc;


    JsonArray array =
        doc.to<JsonArray>();


    array.add(
        "command"
    );

    array.add(
        portName
    );

    array.add(
        "gcode"
    );

    array.add(
        gcode
    );


    String output;


    serializeJson(
        doc,
        output
    );


    Serial.print(
        "[CNCjs] GCODE: "
    );

    Serial.println(
        output
    );


    return socketIO.sendEVENT(
        output
    );
}


// ============================================================
// SEND COMMAND
// ============================================================

bool CNCjsClientCore::sendCommand(
    const String& command
)
{
    if (!lock())
    {
        return false;
    }


    bool result =
        sendCommandInternal(
            command
        );


    unlock();


    return result;
}


// ============================================================
// SEND COMMAND INTERNAL
// ============================================================

bool CNCjsClientCore::sendCommandInternal(
    const String& command
)
{
    if (
        !socketConnectedState ||
        !controllerReadyState
    )
    {
        return false;
    }


    if (
        activeControllerPortState.length() == 0
    )
    {
        return false;
    }


    JsonDocument doc;


    JsonArray array =
        doc.to<JsonArray>();


    array.add(
        "command"
    );

    array.add(
        activeControllerPortState
    );

    array.add(
        command
    );


    String output;


    serializeJson(
        doc,
        output
    );


    Serial.print(
        "[CNCjs] COMMAND: "
    );

    Serial.println(
        output
    );


    return socketIO.sendEVENT(
        output
    );
}


// ============================================================
// SEND REALTIME
// ============================================================

bool CNCjsClientCore::sendRealtime(
    uint8_t command
)
{
    if (!lock())
    {
        return false;
    }


    bool result =
        sendRealtimeInternal(
            command
        );


    unlock();


    return result;
}


// ============================================================
// SEND REALTIME INTERNAL
// ============================================================

bool CNCjsClientCore::sendRealtimeInternal(
    uint8_t command
)
{
    if (
        !socketConnectedState
    )
    {
        return false;
    }


    if (
        !controllerReadyState
    )
    {
        return false;
    }


    if (
        activeControllerPortState.length() == 0
    )
    {
        return false;
    }


    JsonDocument doc;


    JsonArray array =
        doc.to<JsonArray>();


    array.add(
        "command"
    );

    array.add(
        activeControllerPortState
    );


    if (
        command == '!' ||
        command == '~'
    )
    {
        char realtimeCommand[2];


        realtimeCommand[0] =
            static_cast<char>(
                command
            );


        realtimeCommand[1] =
            '\0';


        array.add(
            realtimeCommand
        );
    }
    else
    {
        /*
            The current CNCjs interface only supports the
            realtime commands used by feed hold and resume.
        */

        Serial.print(
            "[CNCjs] Unsupported realtime byte: 0x"
        );

        Serial.println(
            command,
            HEX
        );

        return false;
    }


    String output;


    serializeJson(
        doc,
        output
    );


    Serial.print(
        "[CNCjs] REALTIME: "
    );

    Serial.println(
        output
    );


    return socketIO.sendEVENT(
        output
    );
}