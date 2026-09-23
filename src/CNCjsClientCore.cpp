#include "CNCjsClientCore.h"

#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClient.h>

CNCjsClientCore* CNCjsClientCore::instance =
    nullptr;

// ============================================================
// BEGIN
// ============================================================

void CNCjsClientCore::begin()
{
    instance =
        this;


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

    selectedControllerIndex =
        -1;

    selectedPortIndex =
        -1;

    selectedControllerNameState =
        "";

    selectedPortNameState =
        "";

    activeControllerPortState =
        "";

    activeControllerTypeState =
        "";

    activeControllerBaudrateState =
        0;

    lastCncjsActivity =
        0;

    lastMachineStateTime =
        0;

    lastHeartbeatPing =
        0;


    /*
        Controller identification is a session-level mechanism.

        It is deliberately reset only during begin(), so a later
        connection loss does not cause another automatic
        controller selection.
    */

    controllerVerifiedThisSession_ =
        false;

    controllerCorrectionAttemptedThisSession_ =
        false;


    serialportReadHandler_ =
        [this](const JsonArray& array)
        {
            handleControllerIdentification(
                array
            );
        };


    eventDebug_ =
        EventDebugCounters();

    eventDebugWindowStartedAt =
        millis();


    /*
        Bij het starten is er nog geen geldige controller
        en dus ook geen geldige latest state.
    */

    invalidateControllerState();

    invalidateControllerSettings();

    invalidateSenderStatus();

    invalidateJob();


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
// SNAPSHOT
// ============================================================

CNCjsClientCore::CNCjsSnapshot
CNCjsClientCore::snapshot() const
{
    CNCjsSnapshot result;


    if (
        !lock()
    )
    {
        result.status =
            CNCjsStatus::Error;

        return result;
    }


    result.status =
        currentStatus_;

    result.wifiConnected =
        networkManager_.wifiConnected();

    result.authenticated =
        authenticatedState;

    result.socketConnected =
        socketConnectedState;

    result.portCount =
        numberOfPorts;

    result.controllerCount =
        numberOfControllers;

    result.controllerSelectionReady =
        controllerSelectionReadyState;

    result.selectedController =
        selectedControllerIndex;

    result.selectedPort =
        selectedPortIndex;

    result.selectedControllerName =
        selectedControllerNameState;

    result.selectedPortName =
        selectedPortNameState;

    result.controllerReady =
        controllerReadyState;

    result.controllerSettingsReadyState =
        controllerSettingsReadyState;

    result.controllerPort =
        activeControllerPortState;

    result.controllerType =
        activeControllerTypeState;

    result.controllerBaudrate =
        activeControllerBaudrateState;


    unlock();


    return result;
}


// ============================================================
// CONTROLLER STATE SNAPSHOT
// ============================================================

ControllerStateSnapshot
CNCjsClientCore::controllerStateSnapshot() const
{
    ControllerStateSnapshot snapshot;


    controllerStateBuffer_.acquire(
        snapshot
    );


    return snapshot;
}


// ============================================================
// INVALIDATE CONTROLLER STATE
// ============================================================

void CNCjsClientCore::invalidateControllerState()
{
    ControllerStateSnapshot snapshot;


    snapshot.invalidate();


    controllerStateBuffer_.publish(
        snapshot
    );
}


// ============================================================
// CONTROLLER SETTINGS SNAPSHOT
// ============================================================

ControllerSettingsSnapshot
CNCjsClientCore::controllerSettingsSnapshot() const
{
    ControllerSettingsSnapshot snapshot;


    controllerSettingsBuffer_.acquire(
        snapshot
    );


    return snapshot;
}

void CNCjsClientCore::confirmControllerSettingsPublished()
{
    controllerSettingsReadyState = true;
    Serial.println("confirmControllerSettingsPublished");

    updateControllerReadyState();
}

// ============================================================
// INVALIDATE CONTROLLER SETTINGS
// ============================================================

void CNCjsClientCore::invalidateControllerSettings()
{
    ControllerSettingsSnapshot snapshot;


    snapshot.invalidate();


    controllerSettingsBuffer_.publish(
        snapshot
    );
}


// ============================================================
// SENDER STATUS SNAPSHOT
// ============================================================

SenderStatusSnapshot
CNCjsClientCore::senderStatusSnapshot() const
{
    SenderStatusSnapshot snapshot;


    SenderStatusBuffer_.acquire(
        snapshot
    );


    return snapshot;
}

JobSnapshot
CNCjsClientCore::jobSnapshot() const
{
    JobSnapshot snapshot;


    jobBuffer_.acquire(
        snapshot
    );


    return snapshot;
}


// ============================================================
// INVALIDATE FEED STATUS
// ============================================================

void CNCjsClientCore::invalidateSenderStatus()
{
    SenderStatusSnapshot snapshot;


    snapshot.invalidate();


    SenderStatusBuffer_.publish(
        snapshot
    );
}

void CNCjsClientCore::invalidateJob()
{
    JobSnapshot snapshot;


    snapshot.invalidate();


    jobBuffer_.publish(
        snapshot
    );
}


// ============================================================
// LOCK
// ============================================================

bool CNCjsClientCore::lock() const
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

void CNCjsClientCore::unlock() const
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
        eventDebug_.networkTaskCalls++;


        if (
            lock()
        )
        {
            unsigned long socketLoopStartedAt =
                micros();


            socketIO.loop();


            unsigned long socketLoopDuration =
                micros() -
                socketLoopStartedAt;


            eventDebug_.socketLoopCalls++;

            eventDebug_.socketLoopTotalUs +=
                socketLoopDuration;


            if (
                socketLoopDuration >
                eventDebug_.socketLoopMaxUs
            )
            {
                eventDebug_.socketLoopMaxUs =
                    socketLoopDuration;
            }


            updateConnection();

            updateHeartbeat();


            unlock();
        }


        //debugReport();


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
    if (
        !lock()
    )
    {
        return CNCjsStatus::Error;
    }


    CNCjsStatus result =
        currentStatus_;


    unlock();


    return result;
}


// ============================================================
// CONNECTION STATE
// ============================================================

void CNCjsClientCore::enterConnectionState(
    ConnectionState state
)
{
    ConnectionState previousState =
        connectionState;


    connectionState =
        state;

    stateStartedAt =
        millis();


    /*
        TEMPORARY DEBUG

        Log iedere overgang van de interne connection state.

        Hiermee kunnen we precies zien waardoor de pendant
        uiteindelijk in Offline terechtkomt.
    */

    Serial.print(
        "[CNCjs] Connection state: "
    );

    Serial.print(
        static_cast<int>(previousState)
    );

    Serial.print(
        " -> "
    );

    Serial.print(
        static_cast<int>(state)
    );

    Serial.print(
        " @ "
    );

    Serial.print(
        millis()
    );

    Serial.println(
        " ms"
    );


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

        case ConnectionState::WaitingForControllerSettingsPublication:

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


bool CNCjsClientCore::fetchMacros(
    JsonDocument& document
)
{
    if (
        !lock()
    )
    {
        return false;
    }


    bool result =
        fetchMacrosInternal(
            document
        );

    unlock();


    return result;
}


bool CNCjsClientCore::fetchMacrosInternal(
    JsonDocument& document
)
{
    if(
        !authenticatedState ||
        token.isEmpty()
    )
    {
        return false;
    }

    if(
        !serverIP_
    )
    {
        return false;
    }

    WiFiClient client;
    HTTPClient http;

    String url =
        "http://" +
        serverIP_.toString() +
        ":" +
        String(serverPort_) +
        "/api/macros";

    if(
        !http.begin(
            client,
            url
        )
    )
    {
        return false;
    }

    String authorization =
        "Bearer " +
        token;

    http.addHeader(
        "Authorization",
        authorization
    );

    int httpCode =
        http.GET();

    if(
        httpCode !=
        HTTP_CODE_OK
    )
    {
        http.end();
        return false;
    }

    String response =
        http.getString();

    http.end();

    DeserializationError error =
        deserializeJson(
            document,
            response
        );

    if(
        error
    )
    {
        return false;
    }

    return true;
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
            else if (millis() - stateStartedAt >= WIFI_TIMEOUT)
            {
                networkManager_.advanceWiFiNetwork();
                connectionFailed("WiFi timeout");
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

            break;

        case ConnectionState::WaitingForControllerSettingsPublication:

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

    controllerSettingsReadyState =
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


    lastCncjsActivity =
        0;

    lastHeartbeatPing =
        0;


    invalidateControllerState();

    invalidateControllerSettings();

    invalidateSenderStatus();

    invalidateJob();

    enterConnectionState(
        ConnectionState::Backoff
    );
}


// ============================================================
// WIFI STATUS
// ============================================================

bool CNCjsClientCore::wifiConnected() const
{
    if (
        !lock()
    )
    {
        return false;
    }


    bool result =
        networkManager_.wifiConnected();


    unlock();


    return result;
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
    if (
        !lock()
    )
    {
        return false;
    }


    bool result =
        authenticatedState;


    unlock();


    return result;
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
    if (
        !lock()
    )
    {
        return false;
    }


    bool result =
        socketConnectedState;


    unlock();


    return result;
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
// CNCjs ACTIVITY RECEIVED
// ============================================================

void CNCjsClientCore::cncjsActivityReceived()
{
    lastCncjsActivity =
        millis();
}


// ============================================================
// MACHINE HEARTBEAT RECEIVED
// ============================================================

void CNCjsClientCore::machineHeartbeatReceived()
{
    lastMachineStateTime =
        millis();
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


    /*
        Controller heartbeat
        --------------------

        controller:state is het echte teken van leven.

        Zolang we regelmatig controller:state ontvangen,
        hoeven we niets te doen.

        Pas wanneer de controller gedurende
        TOLERABLE_SILENCE_DURATION stil is geweest,
        vragen we actief om een statusreport.

        lastHeartbeatPing voorkomt dat we bij iedere
        network-task-iteratie opnieuw proberen te pingen.

        Als de controller daarna weer een controller:state
        stuurt, wordt lastCncjsActivity opnieuw gezet en
        begint de stilteperiode opnieuw.
    */

    if (
        now - lastCncjsActivity >=
        TOLERABLE_SILENCE_DURATION
    )
    {
        if (
            now - lastHeartbeatPing >=
            TOLERABLE_SILENCE_DURATION
        )
        {
            heartbeatPing();
        }
    }


    /*
        Machine-state watchdog
        ----------------------

        Dit is bewust een volledig onafhankelijk mechanisme.

        Deze watchdog kijkt niet naar CNCjs-activiteit,
        maar naar echte machine-state activiteit.
    */

    if (
        now - lastMachineStateTime >=
        HEARTBEAT_TIMEOUT
    )
    {
        invalidateControllerState();
    }
}


// ============================================================
// HEARTBEAT PING
// ============================================================

bool CNCjsClientCore::heartbeatPing()
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


#ifdef SOCKETIO_DEBUG

    Serial.print(
        "[CNCjs] HEARTBEAT PING: "
    );

    Serial.println(
        output
    );

#endif


    bool sent =
        socketIO.sendEVENT(
            output
        );


    if (
        sent
    )
    {
        lastHeartbeatPing =
            millis();
    }


    return sent;
}


// ============================================================
// REOPEN ACTIVE CONTROLLER
// ============================================================

bool CNCjsClientCore::reopenActiveController()
{
    if (
        !socketConnectedState
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


    if (
        activeControllerTypeState.length() == 0
    )
    {
        return false;
    }


    Serial.print(
        "[CNCjs] Reopening active controller: "
    );

    Serial.print(
        activeControllerTypeState
    );

    Serial.print(
        " on "
    );

    Serial.println(
        activeControllerPortState
    );


    return openControllerInternal(
        activeControllerPortState.c_str(),
        activeControllerTypeState.c_str(),
        activeControllerBaudrateState
    );
}

void CNCjsClientCore::updateControllerReadyState()
{
    if (
        controllerReadyState &&
        controllerSettingsReadyState
    )
    {
        enterConnectionState(
            ConnectionState::Ready
        );

        lastMachineStateTime =
            millis();

        lastCncjsActivity =
            millis();

        lastHeartbeatPing =
            millis();
#ifdef IOC_DEBUG

        Serial.println();
        Serial.println(
            "[CNCjs] Controller READY"
        );

#endif
    }
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
                connectionState == ConnectionState::SocketConnecting
            )
            {
                Serial.println("[Socket.IO] Initial disconnect ignored");
                break;
            }

            socketConnectedState =
                false;

            controllerReadyState =
                false;

            controllerSettingsReadyState =
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

            lastCncjsActivity =
                0;

            lastHeartbeatPing =
                0;

            currentStatus_ =
                CNCjsStatus::Offline;

            invalidateControllerState();

            invalidateControllerSettings();

            invalidateSenderStatus();

            invalidateJob();

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

            controllerSettingsReadyState =
                false;

            startupReceivedState =
                false;

            portListReceivedState =
                false;

            controllerListProcessedState =
                false;

            listRequested =
                false;

            lastHeartbeatPing =
                millis();


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
#ifdef SOCKETIO_DEBUG

            Serial.print(
                "[IOc] EVENT: "
            );

            Serial.write(
                payload,
                length
            );

            Serial.println();

#endif

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


            // debugEvent(
            //     eventName,
            //     length
            // );


            /*
                Any valid CNCjs event is communication activity.

                The event does not need to have any particular
                semantic meaning for the watchdog.
            */

            cncjsActivityReceived();


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
                Serial.print(
                    "[DEBUG] serialport:open received, connectionState="
                );

                Serial.println(
                    static_cast<int>(connectionState)
                );
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

                    controllerSettingsReadyState =
                        false;


                    invalidateControllerState();
                    invalidateControllerSettings();
                    invalidateSenderStatus();
                    invalidateJob();

                    enterConnectionState(
                        ConnectionState::WaitingForControllerSettingsPublication
                    );
                }

                break;
            }

            // ------------------------------------------------
            // CONTROLLER SETTINGS
            // ------------------------------------------------

            if(
                strcmp(
                    eventName,
                    "controller:settings"
                ) == 0
            )
            {
                Serial.print(
    "[CNCjs] RAW controller:settings @ "
);

Serial.print(
    millis()
);

Serial.println(" ms:");

serializeJson(
    array,
    Serial
);

Serial.println();
                const char* controllerType =
                    array[1];

                JsonObject settings =
                    array[2]["settings"];

                Serial.println();
                Serial.println(
                    "[CNCjs] ========================================"
                );
                Serial.println(
                    "[CNCjs] controller:settings RECEIVED"
                );

                Serial.print(
                    "[CNCjs] controllerType: "
                );

                if(controllerType != nullptr)
                {
                    Serial.println(
                        controllerType
                    );
                }
                else
                {
                    Serial.println(
                        "<null>"
                    );
                }

                Serial.print(
                    "[CNCjs] settings.isNull(): "
                );

                Serial.println(
                    settings.isNull()
                        ? "YES"
                        : "NO"
                );

                if(!settings.isNull())
                {
                    Serial.print(
                        "[CNCjs] $110: "
                    );
                    Serial.println(
                        settings["$110"].as<float>()
                    );

                    Serial.print(
                        "[CNCjs] $111: "
                    );
                    Serial.println(
                        settings["$111"].as<float>()
                    );

                    Serial.print(
                        "[CNCjs] $112: "
                    );
                    Serial.println(
                        settings["$112"].as<float>()
                    );
                }

                Serial.print(
                    "[CNCjs] controllerSettingsReadyState BEFORE: "
                );

                Serial.println(
                    controllerSettingsReadyState
                        ? "true"
                        : "false"
                );

                ControllerSettingsSnapshot snapshot;

                if(
                    controllerType != nullptr
                )
                {
                    snapshot.controllerType =
                        controllerType;
                }

                if(
                    !settings.isNull()
                )
                {
                    snapshot.settings =
                        settings;
                }

                snapshot.valid =
                    true;

                controllerSettingsBuffer_.publish(
                    snapshot
                );

                controllerSettingsPublicationId_++;

                Serial.println(
                    "[CNCjs] controllerSettingsSnapshot published"
                );

                Serial.print(
                    "[CNCjs] controllerSettings publication ID: "
                );

                Serial.println(
                    controllerSettingsPublicationId_
                );

                Serial.print(
                    "[CNCjs] controllerSettingsReadyState AFTER: "
                );

                Serial.println(
                    controllerSettingsReadyState
                        ? "true"
                        : "false"
                );

                Serial.println(
                    "[CNCjs] Calling updateControllerReadyState()"
                );

                updateControllerReadyState();

                machineHeartbeatReceived();

                Serial.println(
                    "[CNCjs] ========================================"
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
#ifdef IOC_DEBUG

                Serial.println(
                    "[CNCjs] Controller state received"
                );

#endif


                controllerReadyState =
                    true;


                const char* controllerType =
                    array[1];


                JsonObject state =
                    array[2];


                ControllerStateSnapshot snapshot;


                snapshot.valid =
                    true;


                if (
                    controllerType != nullptr
                )
                {
                    snapshot.controllerType =
                        controllerType;
                }


                if (
                    !state.isNull()
                )
                {
                    snapshot.state =
                        state;
                }


                controllerStateBuffer_.publish(
                    snapshot
                );


                updateControllerReadyState();

                machineHeartbeatReceived();

                break;
            }


            // ------------------------------------------------
            // FEED STATUS
            // ------------------------------------------------

            if (
                strcmp(
                    eventName,
                    "sender:status"
                ) == 0
            )
            {
                SenderStatusSnapshot snapshot;


                JsonVariant status =
                    array[1];


                if (
                    !status.isNull()
                )
                {
                    snapshot.status =
                        status;
                }


                snapshot.valid =
                    true;


                SenderStatusBuffer_.publish(
                    snapshot
                );

                machineHeartbeatReceived();

                break;
            }


            // ------------------------------------------------
            // G-CODE UNLOAD
            // ------------------------------------------------

            if (
                strcmp(
                    eventName,
                    "gcode:unload"
                ) == 0
            )
            {
                invalidateJob();

                break;
            }


            // ------------------------------------------------
            // G-CODE LOAD
            // ------------------------------------------------

            if (
                strcmp(
                    eventName,
                    "gcode:load"
                ) == 0
            )
            {
                const char* name =
                    array[1];


                if (
                    name != nullptr
                )
                {
                    JobSnapshot snapshot;


                    snapshot.valid =
                        true;


                    snapshot.name =
                        name;


                    jobBuffer_.publish(
                        snapshot
                    );
                }


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
                machineHeartbeatReceived();

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
                if (
                    serialportReadHandler_
                )
                {
                    serialportReadHandler_(
                        array
                    );
                }

                break;
            }


            break;
        }


        case sIOtype_ACK:

#ifdef IOC_DEBUG

            Serial.print(
                "[IOc] ACK: "
            );

            Serial.write(
                payload,
                length
            );

            Serial.println();

#endif

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
// CONTROLLER IDENTIFICATION HANDLER
// ============================================================

void CNCjsClientCore::handleControllerIdentification(
    const JsonArray& array
)
{
    String detectedController =
        identifyControllerFromStatusReport(
            array
        );


    /*
        Unknown serialport data is not evidence of a wrong
        controller. We simply wait for a usable statusreport.
    */

    if (
        detectedController.length() == 0
    )
    {
        return;
    }


    machineHeartbeatReceived();


    Serial.print(
        "[CNCjs] Controller identified by statusreport: "
    );

    Serial.println(
        detectedController
    );


    /*
        The controller has already been verified during this
        power-on session. From this point onward the normal
        serialport:read handler owns the event stream.
    */

    if (
        controllerVerifiedThisSession_
    )
    {
        serialportReadHandler_ =
            [this](const JsonArray& readArray)
            {
                handleSerialportRead(
                    readArray
                );
            };

        return;
    }


    /*
        The statusreport matches the currently selected
        controller.

        No correction is necessary.
    */

    if (
        detectedController.equals(
            selectedControllerNameState
        )
    )
    {
        Serial.println(
            "[CNCjs] Controller selection verified"
        );


        controllerVerifiedThisSession_ =
            true;


        serialportReadHandler_ =
            [this](const JsonArray& readArray)
            {
                handleSerialportRead(
                    readArray
                );
            };

        return;
    }


    /*
        The statusreport identifies a different controller.

        Only one automatic correction is allowed during a
        session. The stored controller preference is deliberately
        not changed here.
    */

    if (
        controllerCorrectionAttemptedThisSession_
    )
    {
        Serial.println(
            "[CNCjs] Controller mismatch remains after automatic correction"
        );

        return;
    }


    int controllerIndex =
        findControllerByName(
            detectedController.c_str()
        );


    if (
        controllerIndex < 0
    )
    {
        Serial.print(
            "[CNCjs] Detected controller is not available in CNCjs controller list: "
        );

        Serial.println(
            detectedController
        );

        return;
    }


    /*
        Remember that the automatic correction has now been
        attempted. This prevents a second automatic controller
        switch during the same power-on session.
    */

    controllerCorrectionAttemptedThisSession_ =
        true;


    Serial.println();
    Serial.println(
        "[CNCjs] Controller mismatch detected"
    );

    Serial.print(
        "  Selected: "
    );

    Serial.println(
        selectedControllerNameState
    );

    Serial.print(
        "  Detected: "
    );

    Serial.println(
        detectedController
    );


    /*
        Runtime-only correction.

        IMPORTANT:
        The runtime selection is updated here, but nothing is
        written to Preferences.

        Therefore the user's stored controller preference remains
        unchanged for the next power-on.
    */

    selectedControllerIndex =
        controllerIndex;

    selectedControllerNameState =
        controllers[
            controllerIndex
        ];


    Serial.print(
        "[CNCjs] Runtime controller selection changed to: "
    );

    Serial.println(
        selectedControllerNameState
    );


    int baudrate =
        baudrateForController(
            selectedControllerNameState.c_str()
        );


    Serial.println(
        "[CNCjs] Automatically opening detected controller for this session"
    );


    openControllerInternal(
        selectedPortNameState.c_str(),
        selectedControllerNameState.c_str(),
        baudrate
    );
}


// ============================================================
// SERIAL PORT READ HANDLER
// ============================================================

void CNCjsClientCore::handleSerialportRead(
    const JsonArray& array
)
{
    const char* response =
        array[1];


    if (
        response == nullptr
    )
    {
        return;
    }


    /*
        Grbl status reports start with '<'.
    */

    if (
        response[0] == '<'
    )
    {
        machineHeartbeatReceived();

        return;
    }


    /*
        TinyG status reports are JSON objects containing r.sr.
        The normal handler does not need to identify the
        controller anymore; that has already happened during
        this session's initial verification.
    */

    JsonDocument doc;


    DeserializationError error =
        deserializeJson(
            doc,
            response
        );


    if (
        error
    )
    {
        return;
    }


    JsonVariant statusReport =
        doc["r"]["sr"];


    if (
        !statusReport.isNull()
    )
    {
        machineHeartbeatReceived();
    }
}


// ============================================================
// IDENTIFY CONTROLLER FROM STATUS REPORT
// ============================================================

String CNCjsClientCore::identifyControllerFromStatusReport(
    const JsonArray& array
) const
{
    const char* response =
        array[1];


    if (
        response == nullptr
    )
    {
        return "";
    }


    /*
        Grbl status reports have the well-known
        <State|...> format.
    */

    if (
        response[0] == '<'
    )
    {
        return "Grbl";
    }


    /*
        TinyG status reports are JSON and contain:
            {
                "r": {
                    "sr": {
                        ...
                    }
                },
                ...
            }
    */

    JsonDocument doc;


    DeserializationError error =
        deserializeJson(
            doc,
            response
        );


    if (
        error
    )
    {
        return "";
    }


    JsonVariant statusReport =
        doc["r"]["sr"];


    if (
        !statusReport.isNull()
    )
    {
        return "TinyG";
    }


    /*
        No usable controller identity was found.

        This is deliberately not treated as a mismatch.
    */

    return "";
}


// ============================================================
// DEBUG EVENT
// ============================================================

void CNCjsClientCore::debugEvent(
    const char* eventName,
    size_t length
)
{
    eventDebug_.eventTotal++;


    if (
        strcmp(
            eventName,
            "controller:state"
        ) == 0
    )
    {
        eventDebug_.controllerStateEvents++;
    }
    else if (
        strcmp(
            eventName,
            "Grbl:state"
        ) == 0
    )
    {
        eventDebug_.grblStateEvents++;
    }
    else if (
        strcmp(
            eventName,
            "serialport:read"
        ) == 0
    )
    {
        eventDebug_.serialportReadEvents++;
    }
    else if (
        strcmp(
            eventName,
            "serialport:write"
        ) == 0
    )
    {
        eventDebug_.serialportWriteEvents++;
    }
    else
    {
        eventDebug_.otherEvents++;
    }


#ifdef CNCJS_EVENT_DEBUG

    Serial.print(
        "[CNCjs EVT] "
    );

    Serial.print(
        millis()
    );

    Serial.print(
        " ms | "
    );

    Serial.print(
        eventName
    );

    Serial.print(
        " | len="
    );

    Serial.println(
        length
    );

#endif
}


// ============================================================
// DEBUG REPORT
// ============================================================

void CNCjsClientCore::debugReport()
{
    unsigned long now =
        millis();


    unsigned long elapsed =
        now -
        eventDebugWindowStartedAt;


    if (
        elapsed < 1000
    )
    {
        return;
    }


    uint32_t networkTaskCalls =
        eventDebug_.networkTaskCalls;

    uint32_t socketLoopCalls =
        eventDebug_.socketLoopCalls;

    uint32_t totalLoopUs =
        eventDebug_.socketLoopTotalUs;

    uint32_t maxLoopUs =
        eventDebug_.socketLoopMaxUs;


    Serial.println();
    Serial.println(
        "========== CNCjs DEBUG =========="
    );


    Serial.print(
        "Window: "
    );

    Serial.print(
        elapsed
    );

    Serial.println(
        " ms"
    );


    Serial.print(
        "networkTask: "
    );

    Serial.print(
        networkTaskCalls
    );

    Serial.println(
        " calls"
    );


    Serial.print(
        "socketIO.loop: "
    );

    Serial.print(
        socketLoopCalls
    );

    Serial.println(
        " calls"
    );


    if (
        socketLoopCalls > 0
    )
    {
        Serial.print(
            "socketIO.loop avg: "
        );

        Serial.print(
            totalLoopUs /
            socketLoopCalls
        );

        Serial.println(
            " us"
        );
    }


    Serial.print(
        "socketIO.loop max: "
    );

    Serial.print(
        maxLoopUs
    );

    Serial.println(
        " us"
    );


    Serial.print(
        "EVENT total: "
    );

    Serial.println(
        eventDebug_.eventTotal
    );


    Serial.print(
        "  controller:state: "
    );

    Serial.println(
        eventDebug_.controllerStateEvents
    );


    Serial.print(
        "  Grbl:state:       "
    );

    Serial.println(
        eventDebug_.grblStateEvents
    );


    Serial.print(
        "  serialport:read:  "
    );

    Serial.println(
        eventDebug_.serialportReadEvents
    );


    Serial.print(
        "  serialport:write: "
    );

    Serial.println(
        eventDebug_.serialportWriteEvents
    );


    Serial.print(
        "  other:            "
    );

    Serial.println(
        eventDebug_.otherEvents
    );


    Serial.println(
        "=================================="
    );


    eventDebug_ =
        EventDebugCounters();

    eventDebugWindowStartedAt =
        now;
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
    if (
        !lock()
    )
    {
        return false;
    }


    bool result =
        controllerSelectionReadyState;


    unlock();


    return result;
}


// ============================================================
// CONTROLLER COUNT
// ============================================================

int CNCjsClientCore::controllerCount() const
{
    if (
        !lock()
    )
    {
        return 0;
    }


    int result =
        numberOfControllers;


    unlock();


    return result;
}


// ============================================================
// CONTROLLER
// ============================================================

String CNCjsClientCore::controller(
    int index
) const
{
    if (
        !lock()
    )
    {
        return "";
    }


    String result;


    if (
        index >= 0 &&
        index < numberOfControllers
    )
    {
        result =
            controllers[index];
    }


    unlock();


    return result;
}


// ============================================================
// PORT COUNT
// ============================================================

int CNCjsClientCore::portCount() const
{
    if (
        !lock()
    )
    {
        return 0;
    }


    int result =
        numberOfPorts;


    unlock();


    return result;
}


// ============================================================
// PORT
// ============================================================

String CNCjsClientCore::port(
    int index
) const
{
    if (
        !lock()
    )
    {
        return "";
    }


    String result;


    if (
        index >= 0 &&
        index < numberOfPorts
    )
    {
        result =
            ports[index];
    }


    unlock();


    return result;
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
    if (
        !lock()
    )
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
    if (
        !lock()
    )
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
    if (
        !lock()
    )
    {
        return -1;
    }


    int result =
        selectedControllerIndex;


    unlock();


    return result;
}


// ============================================================
// SELECTED CONTROLLER NAME
// ============================================================

String CNCjsClientCore::selectedControllerName() const
{
    if (
        !lock()
    )
    {
        return "";
    }


    String result =
        selectedControllerNameState;


    unlock();


    return result;
}


// ============================================================
// SELECTED PORT
// ============================================================

int CNCjsClientCore::selectedPort() const
{
    if (
        !lock()
    )
    {
        return -1;
    }


    int result =
        selectedPortIndex;


    unlock();


    return result;
}


// ============================================================
// SELECTED PORT NAME
// ============================================================

String CNCjsClientCore::selectedPortName() const
{
    if (
        !lock()
    )
    {
        return "";
    }


    String result =
        selectedPortNameState;


    unlock();


    return result;
}


bool CNCjsClientCore::controllerReady() const
{
    if (
        !lock()
    )
    {
        return false;
    }

    bool result =
        controllerReadyState &&
        controllerSettingsReadyState;

    unlock();

    return result;
}


// ============================================================
// ACTIVE CONTROLLER PORT
// ============================================================

String CNCjsClientCore::controllerPort() const
{
    if (
        !lock()
    )
    {
        return "";
    }


    String result =
        activeControllerPortState;


    unlock();


    return result;
}


// ============================================================
// ACTIVE CONTROLLER TYPE
// ============================================================

String CNCjsClientCore::controllerType() const
{
    if (
        !lock()
    )
    {
        return "";
    }


    String result =
        activeControllerTypeState;


    unlock();


    return result;
}


// ============================================================
// ACTIVE CONTROLLER BAUDRATE
// ============================================================

int CNCjsClientCore::controllerBaudrate() const
{
    if (
        !lock()
    )
    {
        return 0;
    }


    int result =
        activeControllerBaudrateState;


    unlock();


    return result;
}


// ============================================================
// OPEN SELECTED CONTROLLER
// ============================================================

bool CNCjsClientCore::openSelectedController()
{
    if (
        !lock()
    )
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
// OPEN CONTROLLER
// ============================================================

bool CNCjsClientCore::openController(
    const char* port,
    const char* controllerType,
    int baudrate
)
{
    if (
        !lock()
    )
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

    controllerSettingsReadyState =
        false;

    enterConnectionState(
        ConnectionState::OpeningController
    );


    /*
        The previous controller state, settings and feed
        status belong to the previous controller. They must
        not remain visible while the new controller is opening.
    */

    invalidateControllerState();

    invalidateControllerSettings();

    invalidateSenderStatus();

    invalidateJob();

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
    if (
        !lock()
    )
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
    if (
        !lock()
    )
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
        !controllerSettingsReadyState ||
        !controllerReadyState || !socketConnectedState
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

#ifdef CNCJS_EVENT_DEBUG
    Serial.print(
        "[CNCjs] GCODE: "
    );

    Serial.println(
        output
    );
#endif

    return socketIO.sendEVENT(
        output
    );
}


// ============================================================
// SEND COMMAND
// ============================================================

bool CNCjsClientCore::sendCommand(
    const String& command,
    const JsonObjectConst& options
)
{
    if(
        !lock()
    )
    {
        return false;
    }


    bool result =
        sendCommandInternal(
            command,
            options
        );


    unlock();


    return result;
}


// ============================================================
// SEND COMMAND INTERNAL
// ============================================================

bool CNCjsClientCore::sendCommandInternal(
    const String& command,
    const JsonObjectConst& options
)
{
    if(
        !controllerSettingsReadyState || !controllerReadyState || !socketConnectedState
    )
    {
        return false;
    }


    if(
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


    if(
        !options.isNull()
    )
    {
        JsonObject optionsCopy =
            array.add<JsonObject>();

        optionsCopy.set(
            options
        );
    }


    String output;


    serializeJson(
        doc,
        output
    );

#ifdef CNCJS_DEBUG
    Serial.print(
        "[CNCjs] COMMAND: "
    );

    Serial.println(
        output
    );
#endif

    return socketIO.sendEVENT(
        output
    );
}