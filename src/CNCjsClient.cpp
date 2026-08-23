#include "CNCjsClient.h"

#include <ESPmDNS.h>
#include <Preferences.h>

#include "Secrets.h"


CNCjsClient* CNCjsClient::instance =
    nullptr;


// ============================================================
// BEGIN
// ============================================================

void CNCjsClient::begin(
    MachineState& machineState
)
{
    instance =
        this;

    machineState_ =
        &machineState;

    Serial.print(
        "[CNCjs] MachineState attached: "
    );

    Serial.println(
        (uintptr_t)machineState_
    );

    Serial.println();
    Serial.println("================================");
    Serial.println(" CNCjsClient");
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

    listRequested =
        false;

    numberOfControllers =
        0;

    numberOfPorts =
        0;

    commandDirty_ =
        false;

    lastCommandSendTime_ =
        0;

    currentStatus =
        CNCjsStatus::Offline;


    if (
        machineState_ != nullptr
    )
    {
        *machineState_ =
            MachineState();
    }


    loadServerSettings();


    if (
        !connectWiFi()
    )
    {
        Serial.println(
            "[CNCjs] WiFi failed"
        );

        return;
    }


    if (
        !resolveCNCjs()
    )
    {
        Serial.println(
            "[CNCjs] CNCjs server not found"
        );

        return;
    }


    if (
        !authenticate()
    )
    {
        Serial.println(
            "[CNCjs] Authentication failed"
        );

        return;
    }


    connectSocket();
}


// ============================================================
// UPDATE
// ============================================================

void CNCjsClient::update()
{
    /*
        Socket.IO moet zeer regelmatig blijven draaien.

        Dit verzorgt onder andere:
        - ontvangen van events
        - WebSocket verkeer
        - reconnects
        - daadwerkelijk verwerken van frames
    */

    socketIO.loop();


    /*
        Machine commands worden niet meer direct vanuit
        execute() verzonden.

        execute() zet het commando dirty.

        Hier wordt maximaal iedere 50 ms één pending command
        naar CNCjs gestuurd.

        Dit is de 20 Hz verzendlaag uit de vorige architectuur.
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

CNCjsClient::CNCjsStatus
CNCjsClient::status() const
{
    return currentStatus;
}


// ============================================================
// WIFI
// ============================================================

bool CNCjsClient::connectWiFi()
{
    const int MAX_ATTEMPTS =
        3;

    const unsigned long CONNECT_TIMEOUT =
        15000;


    currentStatus =
        CNCjsStatus::WiFiConnecting;


    WiFi.mode(
        WIFI_STA
    );


    for (
        int attempt = 1;
        attempt <= MAX_ATTEMPTS;
        attempt++
    )
    {
        Serial.println();
        Serial.print(
            "[WiFi] Connecting (attempt "
        );

        Serial.print(
            attempt
        );

        Serial.println(
            ")"
        );


        WiFi.disconnect(
            true
        );

        delay(500);


        WiFi.begin(
            WIFI_SSID,
            WIFI_PASSWORD
        );


        unsigned long start =
            millis();


        while (
            WiFi.status() != WL_CONNECTED
        )
        {
            if (
                millis() - start >=
                CONNECT_TIMEOUT
            )
            {
                break;
            }


            delay(250);

            Serial.print(
                "."
            );
        }


        Serial.println();


        if (
            WiFi.status() == WL_CONNECTED
        )
        {
            Serial.print(
                "[WiFi] Connected: "
            );

            Serial.println(
                WiFi.localIP()
            );

            return true;
        }


        Serial.println(
            "[WiFi] Attempt failed, retrying..."
        );


        WiFi.disconnect(
            true
        );

        delay(1000);
    }


    Serial.println(
        "[WiFi] Failed after all attempts"
    );


    currentStatus =
        CNCjsStatus::Offline;


    return false;
}


// ============================================================
// WIFI STATUS
// ============================================================

bool CNCjsClient::wifiConnected() const
{
    return WiFi.status() ==
           WL_CONNECTED;
}


// ============================================================
// RESOLVE CNCJS
// ============================================================

bool CNCjsClient::resolveCNCjs()
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

void CNCjsClient::loadServerSettings()
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

void CNCjsClient::saveServerSettings(
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


    Serial.println();

    Serial.println(
        "[CNCjs] Server settings saved:"
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
// AUTHENTICATE
// ============================================================

bool CNCjsClient::authenticate()
{
    const int maxAttempts =
        3;


    currentStatus =
        CNCjsStatus::Authenticating;

    authenticatedState =
        false;

    token =
        "";


    for (
        int attempt = 1;
        attempt <= maxAttempts;
        attempt++
    )
    {
        Serial.println();
        Serial.println(
            "=== CNCjs signin ==="
        );


        String url =
            String("http://") +
            serverIP_.toString() +
            ":" +
            String(serverPort_) +
            "/api/signin";


        Serial.print(
            "POST "
        );

        Serial.println(
            url
        );


        HTTPClient http;


        http.begin(
            url
        );


        http.addHeader(
            "Content-Type",
            "application/json"
        );


        http.setConnectTimeout(
            5000
        );

        http.setTimeout(
            10000
        );


        String body =
            "{\"token\":\"\"}";


        int httpCode =
            http.POST(
                body
            );


        Serial.print(
            "HTTP status: "
        );

        Serial.println(
            httpCode
        );


        if (
            httpCode == 200
        )
        {
            String response =
                http.getString();


            Serial.println(
                "Response:"
            );

            Serial.println(
                response
            );


            DynamicJsonDocument doc(
                2048
            );


            DeserializationError error =
                deserializeJson(
                    doc,
                    response
                );


            if (
                !error
            )
            {
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
                    Serial.println();
                    Serial.println(
                        "[CNCjs] JWT received"
                    );


                    authenticatedState =
                        true;


                    http.end();

                    return true;
                }


                Serial.println(
                    "[CNCjs] No token in response"
                );
            }
            else
            {
                Serial.print(
                    "[CNCjs] JSON error: "
                );

                Serial.println(
                    error.c_str()
                );
            }
        }
        else
        {
            Serial.print(
                "[CNCjs] HTTP error: "
            );

            Serial.println(
                http.errorToString(
                    httpCode
                )
            );
        }


        http.end();


        if (
            attempt < maxAttempts
        )
        {
            Serial.println(
                "[CNCjs] Retrying authentication..."
            );

            delay(1000);
        }
    }


    Serial.println(
        "[CNCjs] Authentication failed"
    );


    authenticatedState =
        false;

    currentStatus =
        CNCjsStatus::Error;


    return false;
}


// ============================================================
// AUTHENTICATED
// ============================================================

bool CNCjsClient::authenticated() const
{
    return authenticatedState;
}


// ============================================================
// CONNECT SOCKET
// ============================================================

void CNCjsClient::connectSocket()
{
    currentStatus =
        CNCjsStatus::Connecting;


    String path =
        "/socket.io/?token=";

    path +=
        token;

    path +=
        "&EIO=3";


    Serial.println(
        "[Socket.IO] Connecting..."
    );


    socketIO.begin(
        CNCJS_HOST,
        CNCJS_PORT,
        path.c_str()
    );


    socketIO.onEvent(
        CNCjsClient::socketIOEvent
    );
}


// ============================================================
// SOCKET STATUS
// ============================================================

bool CNCjsClient::socketConnected() const
{
    return socketConnectedState;
}


// ============================================================
// SOCKET CALLBACK
// ============================================================

void CNCjsClient::socketIOEvent(
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

MachineStatus CNCjsClient::machineStatusFromCNCjs(
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

void CNCjsClient::updateMachineState(
    JsonObject status,
    JsonObject parserstate
)
{
    Serial.println(
        "[CNCjs] updateMachineState()"
    );

    Serial.print(
        "[CNCjs] machineState pointer: "
    );

    Serial.println(
        (uintptr_t)machineState_
    );


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
}


// ============================================================
// SOCKET EVENT HANDLER
// ============================================================

void CNCjsClient::handleSocketEvent(
    socketIOmessageType_t type,
    uint8_t* payload,
    size_t length
)
{
    switch (type)
    {
        case sIOtype_DISCONNECT:
        {
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

            commandDirty_ =
                false;


            currentStatus =
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

            listRequested =
                false;


            currentStatus =
                CNCjsStatus::WaitingForLists;


            if (
                machineState_ != nullptr
            )
            {
                machineState_->connected =
                    false;

                machineState_->machineStatus =
                    MACHINE_DISCONNECTED;
            }


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


            DynamicJsonDocument doc(
                8192
            );


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


                    currentStatus =
                        CNCjsStatus::OpeningController;


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


                    Serial.println(
                        "[CNCjs] Waiting for controller initialization..."
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


                currentStatus =
                    CNCjsStatus::Ready;


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


            break;
        }


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

void CNCjsClient::requestPortList()
{
    if (
        listRequested
    )
    {
        return;
    }


    DynamicJsonDocument doc(
        256
    );


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

bool CNCjsClient::controllerSelectionReady() const
{
    return controllerSelectionReadyState;
}


// ============================================================
// CONTROLLER COUNT
// ============================================================

int CNCjsClient::controllerCount() const
{
    return numberOfControllers;
}


// ============================================================
// CONTROLLER
// ============================================================

const char* CNCjsClient::controller(
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

int CNCjsClient::portCount() const
{
    return numberOfPorts;
}


// ============================================================
// PORT
// ============================================================

const char* CNCjsClient::port(
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

int CNCjsClient::loadSavedController()
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

String CNCjsClient::loadSavedControllerName()
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

String CNCjsClient::loadSavedPortName()
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

void CNCjsClient::saveSelectedController(
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

void CNCjsClient::saveSelectedPort(
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

int CNCjsClient::findControllerByName(
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

int CNCjsClient::findPortByName(
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

int CNCjsClient::baudrateForController(
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

void CNCjsClient::loadControllerList()
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
            Serial.println(
                "[CNCjs] Automatically opening restored controller..."
            );

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


    currentStatus =
        CNCjsStatus::SelectionRequired;
}


// ============================================================
// CHOOSE CONTROLLER
// ============================================================

void CNCjsClient::chooseController()
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


    currentStatus =
        CNCjsStatus::OpeningController;


    Serial.println(
        "[CNCjs] Automatically opening selected controller..."
    );


    openSelectedController();
}


// ============================================================
// SELECT CONTROLLER
// ============================================================

bool CNCjsClient::selectController(
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


    currentStatus =
        CNCjsStatus::OpeningController;


    Serial.println();
    Serial.println(
        "[CNCjs] Selected controller:"
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


    Serial.println(
        "[CNCjs] Automatically opening selected controller..."
    );


    return openSelectedController();
}


// ============================================================
// SELECTED CONTROLLER
// ============================================================

int CNCjsClient::selectedController() const
{
    return selectedControllerIndex;
}


// ============================================================
// SELECTED CONTROLLER NAME
// ============================================================

const char*
CNCjsClient::selectedControllerName() const
{
    return selectedControllerNameState.c_str();
}


// ============================================================
// SELECTED PORT
// ============================================================

int CNCjsClient::selectedPort() const
{
    return selectedPortIndex;
}


// ============================================================
// SELECTED PORT NAME
// ============================================================

const char*
CNCjsClient::selectedPortName() const
{
    return selectedPortNameState.c_str();
}


// ============================================================
// CONTROLLER READY
// ============================================================

bool CNCjsClient::controllerReady() const
{
    return controllerReadyState;
}


// ============================================================
// ACTIVE CONTROLLER PORT
// ============================================================

const char*
CNCjsClient::controllerPort() const
{
    return activeControllerPortState.c_str();
}


// ============================================================
// ACTIVE CONTROLLER TYPE
// ============================================================

const char*
CNCjsClient::controllerType() const
{
    return activeControllerTypeState.c_str();
}


// ============================================================
// ACTIVE CONTROLLER BAUDRATE
// ============================================================

int CNCjsClient::controllerBaudrate() const
{
    return activeControllerBaudrateState;
}


// ============================================================
// OPEN SELECTED CONTROLLER
// ============================================================

bool CNCjsClient::openSelectedController()
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

bool CNCjsClient::execute(
    const MachineCommand& command
)
{
    if (
        currentStatus != CNCjsStatus::Ready
    )
    {
        Serial.println(
            "[CNCjs] execute rejected: not ready"
        );

        return false;
    }


    switch (
        command.type
    )
    {
        case MACHINE_COMMAND_GCODE:
        {
            /*
                Niet direct verzenden.

                Bewaar het commando en markeer het dirty.

                update() verzendt het vervolgens op 20 Hz.
            */

            pendingCommand_ =
                command;

            commandDirty_ =
                true;


            Serial.println(
                "[CNCjs] MachineCommand queued"
            );


            return true;
        }


        case MACHINE_COMMAND_JOG_CANCEL:
        {
            /*
                Jog cancel moet direct worden uitgevoerd.
                Dit is een realtime/interrupt-achtig commando
                en hoort niet achter een 50 ms plannerqueue.
            */

            return jogCancel();
        }


        case MACHINE_COMMAND_FEED_HOLD:

            return sendRealtime(
                '!'
            );


        case MACHINE_COMMAND_RESUME:

            return sendRealtime(
                '~'
            );


        case MACHINE_COMMAND_RESET:

            return sendRealtime(
                0x18
            );


        case MACHINE_COMMAND_NONE:

        default:

            return false;
    }
}


// ============================================================
// SEND PENDING COMMAND
// ============================================================

bool CNCjsClient::sendPendingCommand()
{
    if (
        !commandDirty_
    )
    {
        return false;
    }


    if (
        !socketConnectedState
    )
    {
        Serial.println(
            "[CNCjs] Pending command waiting:"
            " Socket.IO not connected"
        );

        return false;
    }


    if (
        !controllerReadyState
    )
    {
        Serial.println(
            "[CNCjs] Pending command waiting:"
            " controller not ready"
        );

        return false;
    }


    /*
        Alleen G-code/JOG commands komen momenteel
        via deze queue.
    */

    if (
        pendingCommand_.type !=
        MACHINE_COMMAND_GCODE
    )
    {
        commandDirty_ =
            false;

        return false;
    }


    String command =
        pendingCommand_.command;


    if (
        activeControllerPortState.length() == 0
    )
    {
        Serial.println(
            "[CNCjs] Pending command rejected:"
            " no active controller port"
        );

        return false;
    }


    DynamicJsonDocument doc(
        512
    );


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
        command
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


    Serial.print(
        "[CNCjs] sendEVENT(): "
    );

    Serial.println(
        sent ? "TRUE" : "FALSE"
    );


    /*
        Het commando is nu uit de dirty queue.

        Bij een nieuwe planner-update komt er weer een nieuw
        MachineCommand binnen via execute().
    */

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

bool CNCjsClient::jogCancel()
{
    if (!socketConnectedState)
    {
        Serial.println(
            "[CNCjs] Cannot cancel jog:"
            " Socket.IO not connected"
        );

        return false;
    }


    if (!controllerReadyState)
    {
        Serial.println(
            "[CNCjs] Cannot cancel jog:"
            " controller not ready"
        );

        return false;
    }


    DynamicJsonDocument doc(
        256
    );


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


    socketIO.sendEVENT(
        output
    );


    return true;
}


bool CNCjsClient::feedHold()
{
    if (!socketConnectedState)
    {
        return false;
    }

    if (!controllerReadyState)
    {
        return false;
    }

    // CNCjs feed hold
}

bool CNCjsClient::resume(){}

bool CNCjsClient::reset(){}

// ============================================================
// OPEN CONTROLLER
// ============================================================

bool CNCjsClient::openController(
    const char* portName,
    const char* controllerType,
    int baudrate
)
{
    if (
        !socketConnectedState
    )
    {
        Serial.println(
            "[CNCjs] Cannot open controller:"
            " Socket.IO not connected"
        );

        return false;
    }


    if (
        portName == nullptr ||
        controllerType == nullptr
    )
    {
        Serial.println(
            "[CNCjs] Cannot open controller:"
            " invalid parameters"
        );

        return false;
    }


    controllerReadyState =
        false;


    currentStatus =
        CNCjsStatus::OpeningController;


    if (
        machineState_ != nullptr
    )
    {
        machineState_->connected =
            false;

        machineState_->machineStatus =
            MACHINE_DISCONNECTED;
    }


    DynamicJsonDocument doc(
        512
    );


    JsonArray array =
        doc.to<JsonArray>();


    array.add(
        "open"
    );


    array.add(
        portName
    );


    JsonObject options =
        array.createNestedObject();


    options["controllerType"] =
        controllerType;


    options["baudrate"] =
        baudrate;


    options["rtscts"] =
        false;


    JsonObject pin =
        options.createNestedObject(
            "pin"
        );


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


    bool sent =
        socketIO.sendEVENT(
            output
        );


    Serial.print(
        "[CNCjs] sendEVENT(): "
    );

    Serial.println(
        sent ? "TRUE" : "FALSE"
    );


    return sent;
}


// ============================================================
// SEND GCODE - ACTIVE CONTROLLER
// ============================================================

bool CNCjsClient::sendGcode(
    const char* gcode
)
{
    if (
        !controllerReadyState
    )
    {
        Serial.println(
            "[CNCjs] Cannot send G-code:"
            " controller not ready"
        );

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

bool CNCjsClient::sendGcode(
    const char* portName,
    const char* gcode
)
{
    if (
        !socketConnectedState
    )
    {
        Serial.println(
            "[CNCjs] Cannot send G-code:"
            " Socket.IO not connected"
        );

        return false;
    }


    if (
        !controllerReadyState
    )
    {
        Serial.println(
            "[CNCjs] Cannot send G-code:"
            " controller not ready"
        );

        return false;
    }


    if (
        portName == nullptr ||
        gcode == nullptr
    )
    {
        return false;
    }


    DynamicJsonDocument doc(
        512
    );


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


    bool sent =
        socketIO.sendEVENT(
            output
        );


    Serial.print(
        "[CNCjs] sendEVENT(): "
    );

    Serial.println(
        sent ? "TRUE" : "FALSE"
    );


    return sent;
}






// ============================================================
// SEND COMMAND
// ============================================================

bool CNCjsClient::sendCommand(
    const String& command
)
{
    if (
        !socketConnectedState
    )
    {
        Serial.println(
            "[CNCjs] Cannot send command:"
            " Socket.IO not connected"
        );

        return false;
    }


    if (
        !controllerReadyState
    )
    {
        Serial.println(
            "[CNCjs] Cannot send command:"
            " controller not ready"
        );

        return false;
    }


    if (
        activeControllerPortState.length() == 0
    )
    {
        Serial.println(
            "[CNCjs] Cannot send command:"
            " no active controller port"
        );

        return false;
    }


    DynamicJsonDocument doc(
        512
    );


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


    bool sent =
        socketIO.sendEVENT(
            output
        );


    Serial.print(
        "[CNCjs] sendEVENT(): "
    );

    Serial.println(
        sent ? "TRUE" : "FALSE"
    );


    return sent;
}


// ============================================================
// SEND REALTIME
// ============================================================

bool CNCjsClient::sendRealtime(
    uint8_t command
)
{
    if (
        !socketConnectedState
    )
    {
        Serial.println(
            "[CNCjs] Cannot send realtime:"
            " Socket.IO not connected"
        );

        return false;
    }


    if (
        !controllerReadyState
    )
    {
        Serial.println(
            "[CNCjs] Cannot send realtime:"
            " controller not ready"
        );

        return false;
    }


    if (
        activeControllerPortState.length() == 0
    )
    {
        Serial.println(
            "[CNCjs] Cannot send realtime:"
            " no active controller port"
        );

        return false;
    }


    DynamicJsonDocument doc(
        256
    );


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
            static_cast<char>(command);

        realtimeCommand[1] =
            '\0';


        array.add(
            realtimeCommand
        );
    }
    else
    {
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


    bool sent =
        socketIO.sendEVENT(
            output
        );


    Serial.print(
        "[CNCjs] sendEVENT(): "
    );

    Serial.println(
        sent ? "TRUE" : "FALSE"
    );


    return sent;
}