#include "CNCjsClientCore.h"

#include <ESPmDNS.h>
#include <Preferences.h>

#include "Secrets.h"


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


    /*
        Reset runtime state.
    */

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


    loadServerSettings();


    /*
        begin() doet vanaf nu GEEN netwerkoperatie.

        De verbinding wordt volledig vanuit update()
        opgebouwd.
    */

    enterConnectionState(
        ConnectionState::Start
    );
}


// ============================================================
// UPDATE
// ============================================================

void CNCjsClientCore::update()
{
    /*
        Socket.IO moet altijd blijven draaien.
    */

    socketIO.loop();


    /*
        Eerst de verbinding/state machine.
    */

    updateConnection();


    /*
        Vervolgens machine heartbeat.
    */

    updateHeartbeat();


    /*
        Tenslotte eventueel een queued machine command.
    */

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


    switch(state)
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
    switch(connectionState)
    {
        // ----------------------------------------------------
        // START
        // ----------------------------------------------------

        case ConnectionState::Start:

            if (
                WiFi.status() ==
                WL_CONNECTED
            )
            {
                enterConnectionState(
                    ConnectionState::Resolving
                );
            }
            else
            {
                startWiFiConnection();

                enterConnectionState(
                    ConnectionState::WiFiConnecting
                );
            }

            break;


        // ----------------------------------------------------
        // WIFI
        // ----------------------------------------------------

        case ConnectionState::WiFiConnecting:

            if (
                updateWiFiConnection()
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


        // ----------------------------------------------------
        // DNS
        // ----------------------------------------------------

        case ConnectionState::Resolving:

            /*
                hostByName() is normally very short-lived for
                cncjs.local. Once resolved, everything after
                this point is asynchronous.

                We deliberately perform it only once per
                connection attempt.
            */

            if (
                resolveCNCjs()
            )
            {
                resetAuthentication();

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


        // ----------------------------------------------------
        // AUTHENTICATION
        // ----------------------------------------------------

        case ConnectionState::Authenticating:

            if (
                !authRequestSent
            )
            {
                startAuthentication();
            }


            if (
                updateAuthentication()
            )
            {
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


        // ----------------------------------------------------
        // SOCKET
        // ----------------------------------------------------

        case ConnectionState::SocketConnecting:

            /*
                Start Socket.IO slechts één keer.

                socketBeginRequested wordt bij iedere nieuwe
                connection attempt gereset door de state-machine.
            */

            if (
                !socketBeginRequested
            )
            {
                connectSocket();
            }


            /*
                Vanaf hier is Socket.IO volledig asynchroon.

                De socket callback zet socketConnectedState
                zodra sIOtype_CONNECT wordt ontvangen.
            */

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


        // ----------------------------------------------------
        // LISTS
        // ----------------------------------------------------

        case ConnectionState::WaitingForLists:

            if (
                startupReceivedState &&
                portListReceivedState
            )
            {
                loadControllerList();
            }

            break;


        // ----------------------------------------------------
        // OPENING CONTROLLER
        // ----------------------------------------------------

        case ConnectionState::OpeningController:

            /*
                serialport:open moves us into this state.

                controller:state subsequently makes the
                controller ready.
            */

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


        // ----------------------------------------------------
        // READY
        // ----------------------------------------------------

        case ConnectionState::Ready:

            /*
                If Socket.IO disappears, the socket event
                handler will move us back to Offline.
            */

            break;


        // ----------------------------------------------------
        // BACKOFF
        // ----------------------------------------------------

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
// WIFI START
// ============================================================

void CNCjsClientCore::startWiFiConnection()
{
    Serial.println();

    Serial.println(
        "[WiFi] Starting connection..."
    );


    WiFi.mode(
        WIFI_STA
    );


    WiFi.disconnect(
        false
    );


    WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
    );
}


// ============================================================
// WIFI UPDATE
// ============================================================

bool CNCjsClientCore::updateWiFiConnection()
{
    if (
        WiFi.status() !=
        WL_CONNECTED
    )
    {
        return false;
    }


    Serial.print(
        "[WiFi] Connected: "
    );

    Serial.println(
        WiFi.localIP()
    );


    return true;
}


// ============================================================
// WIFI STATUS
// ============================================================

bool CNCjsClientCore::wifiConnected() const
{
    return WiFi.status() ==
           WL_CONNECTED;
}


// ============================================================
// RESOLVE CNCJS
// ============================================================

bool CNCjsClientCore::resolveCNCjs()
{
    Serial.println();

    Serial.print(
        "[CNCjs] Resolving "
    );

    Serial.print(
        serverHost_
    );

    Serial.print(
        ":"
    );

    Serial.println(
        serverPort_
    );


    if (
        WiFi.hostByName(
            serverHost_.c_str(),
            serverIP_
        ) != 1
    )
    {
        Serial.println(
            "[CNCjs] DNS/mDNS resolution failed"
        );

        return false;
    }


    Serial.print(
        "[CNCjs] Resolved to: "
    );

    Serial.println(
        serverIP_
    );


    return true;
}


// ============================================================
// LOAD SERVER SETTINGS
// ============================================================

void CNCjsClientCore::loadServerSettings()
{
    Preferences preferences;


    preferences.begin(
        "cncjs",
        true
    );


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
        "  Host: "
    );

    Serial.println(
        serverHost_
    );


    Serial.print(
        "  Port: "
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


    preferences.begin(
        "cncjs",
        false
    );


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
// AUTHENTICATION RESET
// ============================================================

void CNCjsClientCore::resetAuthentication()
{
    authClient.stop();

    authResponse =
        "";

    authHeaderBuffer =
        "";

    authRequestSent =
        false;

    authHeadersReceived =
        false;

    authContentLength =
        -1;

    authStartedAt =
        0;

    token =
        "";
}


// ============================================================
// AUTHENTICATION START
// ============================================================

bool CNCjsClientCore::startAuthentication()
{
    resetAuthentication();


    Serial.println();
    Serial.println(
        "=== CNCjs signin ==="
    );


    if (
        !authClient.connect(
            serverIP_,
            serverPort_
        )
    )
    {
        Serial.println(
            "[CNCjs] Authentication TCP connection failed"
        );

        return false;
    }


    String body =
        "{\"token\":\"\"}";


    authClient.print(
        "POST /api/signin HTTP/1.1\r\n"
    );


    authClient.print(
        "Host: "
    );

    authClient.print(
        serverHost_
    );

    authClient.print(
        "\r\n"
    );


    authClient.print(
        "Content-Type: application/json\r\n"
    );


    authClient.print(
        "Content-Length: "
    );

    authClient.print(
        body.length()
    );

    authClient.print(
        "\r\n"
    );


    authClient.print(
        "Connection: close\r\n"
    );


    authClient.print(
        "\r\n"
    );


    authClient.print(
        body
    );


    authRequestSent =
        true;

    authStartedAt =
        millis();


    Serial.println(
        "[CNCjs] Authentication request sent"
    );


    return true;
}


// ============================================================
// AUTHENTICATION UPDATE
// ============================================================

bool CNCjsClientCore::updateAuthentication()
{
    if (
        !authRequestSent
    )
    {
        return false;
    }


    while (
        authClient.available()
    )
    {
        char c =
            static_cast<char>(
                authClient.read()
            );


        authResponse +=
            c;


        /*
            Header terminator.
        */

        if (
            !authHeadersReceived &&
            authResponse.endsWith(
                "\r\n\r\n"
            )
        )
        {
            authHeadersReceived =
                true;


            int contentLengthPosition =
                authResponse.indexOf(
                    "Content-Length:"
                );


            if (
                contentLengthPosition >= 0
            )
            {
                int lineEnd =
                    authResponse.indexOf(
                        "\r\n",
                        contentLengthPosition
                    );


                if (
                    lineEnd >= 0
                )
                {
                    String line =
                        authResponse.substring(
                            contentLengthPosition,
                            lineEnd
                        );


                    int colon =
                        line.indexOf(
                            ':'
                        );


                    if (
                        colon >= 0
                    )
                    {
                        authContentLength =
                            line.substring(
                                colon + 1
                            ).toInt();
                    }
                }
            }
        }
    }


    if (
        authHeadersReceived
    )
    {
        String body;


        if (
            extractAuthenticationBody(
                body
            )
        )
        {
            JsonDocument doc;


            DeserializationError error =
                deserializeJson(
                    doc,
                    body
                );


            if (
                error
            )
            {
                Serial.print(
                    "[CNCjs] Authentication JSON error: "
                );

                Serial.println(
                    error.c_str()
                );


                authClient.stop();

                return false;
            }


            if (
                doc["token"].is<const char*>()
            )
            {
                token =
                    doc["token"].as<String>();
            }
            else if (
                doc["accessToken"].is<const char*>()
            )
            {
                token =
                    doc["accessToken"].as<String>();
            }


            if (
                token.length() > 0
            )
            {
                authenticatedState =
                    true;


                authClient.stop();


                Serial.println(
                    "[CNCjs] JWT received"
                );


                return true;
            }


            Serial.println(
                "[CNCjs] Authentication response contained no token"
            );


            authClient.stop();

            return false;
        }
    }


    return false;
}


// ============================================================
// EXTRACT AUTHENTICATION BODY
// ============================================================

bool CNCjsClientCore::extractAuthenticationBody(
    String& body
)
{
    int separator =
        authResponse.indexOf(
            "\r\n\r\n"
        );


    if (
        separator < 0
    )
    {
        return false;
    }


    int bodyStart =
        separator + 4;


    int bodyLength =
        authResponse.length() -
        bodyStart;


    if (
        authContentLength >= 0 &&
        bodyLength < authContentLength
    )
    {
        return false;
    }


    if (
        authContentLength >= 0
    )
    {
        body =
            authResponse.substring(
                bodyStart,
                bodyStart +
                authContentLength
            );
    }
    else
    {
        body =
            authResponse.substring(
                bodyStart
            );
    }


    return body.length() > 0;
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


    /*
        Periodiek een GRBL statusreport vragen.
    */

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


    /*
        Geen machine-status ontvangen binnen de timeout:
        machine is stale/disconnected.

        De Socket.IO verbinding hoeft hiervoor niet
        onmiddellijk verbroken te worden.
    */

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


    /*
        Dit is exact het CNCjs command dat we inmiddels
        succesvol getest hebben:

        ["command","/dev/ttyUSB0","statusreport"]

        CNCjs vertaalt dit naar "?" richting GRBL.
    */

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


    bool sent =
        socketIO.sendEVENT(
            output
        );


    return sent;
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
    switch(type)
    {
        // ----------------------------------------------------
        // DISCONNECT
        // ----------------------------------------------------

        case sIOtype_DISCONNECT:
        {
            /*
                Belangrijk:

                De Socket.IO library kan tijdens socketIO.begin()
                eerst een DISCONNECT-event genereren voordat de
                eerste CONNECT ontvangen is.

                Dat is géén echte verbroken verbinding.

                Alleen wanneer de socket al daadwerkelijk
                verbonden was, behandelen we DISCONNECT als een
                echte connection loss.
            */

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


        // ----------------------------------------------------
        // CONNECT
        // ----------------------------------------------------

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


        // ----------------------------------------------------
        // EVENT
        // ----------------------------------------------------

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


            // =================================================
            // STARTUP
            // =================================================

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
                    portListReceivedState
                )
                {
                    loadControllerList();
                }


                break;
            }


            // =================================================
            // SERIALPORT LIST
            // =================================================

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
                    portListReceivedState
                )
                {
                    loadControllerList();
                }


                break;
            }


            // =================================================
            // SERIALPORT OPEN
            // =================================================

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


            // =================================================
            // CONTROLLER SETTINGS
            // =================================================

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


            // =================================================
            // CONTROLLER STATE
            // =================================================

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


            // =================================================
            // GRBL STATE
            // =================================================

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


            // =================================================
            // SERIALPORT READ
            // =================================================

            if (
                strcmp(
                    eventName,
                    "serialport:read"
                ) == 0
            )
            {
                /*
                    CNCjs geeft hier de ruwe response van GRBL
                    door.

                    Bijvoorbeeld:

                    <Idle|MPos:0.000,0.000,0.000|FS:0,0|Ov:100,100,100>

                    Dit is belangrijk voor onze heartbeat:
                    een ontvangen GRBL-status betekent dat de
                    machine daadwerkelijk antwoordt.

                    We gebruiken dit dus als teken dat de
                    machine online is.
                */

                const char* response =
                    array[1];


                if (
                    response == nullptr
                )
                {
                    break;
                }


                /*
                    Alleen echte GRBL statusresponses tellen
                    mee als machine heartbeat.

                    Andere serialport:read berichten kunnen
                    bijvoorbeeld startup/output zijn.
                */

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


                /*
                    Parse de belangrijkste informatie uit de
                    ruwe GRBL statusregel.

                    We houden dit bewust beperkt:
                    de volledige CNCjs "Grbl:state" blijft de
                    voorkeursbron voor de uitgebreide state.
                */

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


                /*
                    Parse MPos.

                    Voorbeeld:

                    MPos:0.000,0.000,0.000
                */

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


                /*
                    Parse feedrate.

                    Bijvoorbeeld:

                    FS:0,0

                    Het eerste getal is de actuele feedrate.
                */

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


                /*
                    Parse spindle override / status indien
                    later gewenst.

                    Voor nu laten we spindleSpeed ongemoeid:
                    de uitgebreide Grbl:state event is daarvoor
                    de betrouwbare bron.
                */


                break;
            }


            break;
        }


        // ----------------------------------------------------
        // ACK
        // ----------------------------------------------------

        case sIOtype_ACK:
        {
            Serial.print(
                "[IOc] ACK: "
            );

            Serial.write(
                payload,
                length
            );

            Serial.println();

            break;
        }


        // ----------------------------------------------------
        // ERROR
        // ----------------------------------------------------

        case sIOtype_ERROR:
        {
            Serial.print(
                "[IOc] ERROR: "
            );

            Serial.write(
                payload,
                length
            );

            Serial.println();

            break;
        }


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

const char* CNCjsClientCore::controller(
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

const char* CNCjsClientCore::port(
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


    preferences.begin(
        "cncjs",
        true
    );


    int index =
        preferences.getInt(
            "controllerIndex",
            -1
        );


    preferences.end();


    return index;
}


// ============================================================
// LOAD SAVED CONTROLLER NAME
// ============================================================

String CNCjsClientCore::loadSavedControllerName()
{
    Preferences preferences;


    preferences.begin(
        "cncjs",
        true
    );


    String name =
        preferences.getString(
            "controllerName",
            ""
        );


    preferences.end();


    return name;
}


// ============================================================
// LOAD SAVED PORT NAME
// ============================================================

String CNCjsClientCore::loadSavedPortName()
{
    Preferences preferences;


    preferences.begin(
        "cncjs",
        true
    );


    String name =
        preferences.getString(
            "portName",
            ""
        );


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


    preferences.begin(
        "cncjs",
        false
    );


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


    preferences.begin(
        "cncjs",
        false
    );


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
        name == nullptr
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
        name == nullptr
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
            "[CNCjs] Restored controller:"
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


        if (
            !controllerReadyState
        )
        {
            openSelectedController();
        }


        return;
    }


    Serial.println();
    Serial.println(
        "[CNCjs] Controller selection required."
    );


    controllerSelectionReadyState =
        true;


    currentStatus_ =
        CNCjsStatus::SelectionRequired;


    connectionState =
        ConnectionState::WaitingForLists;
}


// ============================================================
// CHOOSE CONTROLLER
// ============================================================

void CNCjsClientCore::chooseController()
{
    if (
        numberOfControllers == 0 ||
        numberOfPorts == 0
    )
    {
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


    openSelectedController();
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
        portIndex < 0 ||
        portIndex >= numberOfPorts
    )
    {
        return false;
    }


    if (
        controllerIndex < 0 ||
        controllerIndex >= numberOfControllers
    )
    {
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


    return openSelectedController();
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


    return openController(
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
    if (
        connectionState !=
        ConnectionState::Ready
    )
    {
        Serial.println(
            "[CNCjs] execute rejected: not ready"
        );

        return false;
    }


    switch(command.type)
    {
        case MACHINE_COMMAND_GCODE:
        {
            pendingCommand_ =
                command;


            commandDirty_ =
                true;


            return true;
        }


        case MACHINE_COMMAND_JOG_CANCEL:

            return jogCancel();


        case MACHINE_COMMAND_FEED_HOLD:

            return feedHold();


        case MACHINE_COMMAND_RESUME:

            return resume();


        case MACHINE_COMMAND_RESET:

            return reset();


        case MACHINE_COMMAND_NONE:

        default:

            return false;
    }
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
    return sendRealtime(
        '!'
    );
}


// ============================================================
// RESUME
// ============================================================

bool CNCjsClientCore::resume()
{
    return sendRealtime(
        '~'
    );
}


// ============================================================
// RESET
// ============================================================

bool CNCjsClientCore::reset()
{
    /*
        GRBL reset is a realtime 0x18 byte.

        The current sendRealtime() interface intentionally
        only supports commands that CNCjs accepts as a
        one-character command string.

        Keep this explicit for now.
    */

    Serial.println(
        "[CNCjs] Reset requested"
    );


    return sendRealtime(
        0x18
    );
}


// ============================================================
// OPEN CONTROLLER
// ============================================================

bool CNCjsClientCore::openController(
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
    if (
        !controllerReadyState
    )
    {
        return false;
    }


    return sendGcode(
        activeControllerPortState.c_str(),
        gcode
    );
}


// ============================================================
// SEND GCODE - EXPLICIT PORT
// ============================================================

bool CNCjsClientCore::sendGcode(
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
            GRBL reset is 0x18. JSON strings cannot represent
            this as the actual realtime byte.

            CNCjs' "command" event therefore cannot use the
            same path as ! and ~.

            Keep this rejected until we implement the proper
            CNCjs reset event.
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