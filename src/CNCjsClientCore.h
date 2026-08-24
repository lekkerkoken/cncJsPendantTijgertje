#ifndef CNCJS_CLIENT_CORE_H
#define CNCJS_CLIENT_CORE_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <SocketIOclient.h>
#include <ArduinoJson.h>

#include "MachineCommand.h"
#include "MachineState.h"
#include "NetworkManager.h"


class CNCjsClientCore
{
public:

    // ========================================================
    // Lifecycle
    // ========================================================

    void begin(
        MachineState& machineState
    );

    void update();


    // ========================================================
    // Status
    // ========================================================

    enum class CNCjsStatus
    {
        Offline,
        WiFiConnecting,
        Authenticating,
        Connecting,
        WaitingForLists,
        SelectionRequired,
        OpeningController,
        Ready,
        Error
    };


    CNCjsStatus status() const;


    // ========================================================
    // Connection
    // ========================================================

    bool wifiConnected() const;
    bool authenticated() const;
    bool socketConnected() const;


    // ========================================================
    // Serial ports
    // ========================================================

    int portCount() const;

    const char* port(
        int index
    ) const;


    // ========================================================
    // Controllers
    // ========================================================

    int controllerCount() const;

    const char* controller(
        int index
    ) const;


    // ========================================================
    // Controller selection
    // ========================================================

    bool controllerSelectionReady() const;

    int selectedController() const;

    const char* selectedControllerName() const;

    int selectedPort() const;

    const char* selectedPortName() const;


    bool selectController(
        int portIndex,
        int controllerIndex
    );

    void chooseController();


    // ========================================================
    // Active controller
    // ========================================================

    bool controllerReady() const;

    const char* controllerPort() const;

    const char* controllerType() const;

    int controllerBaudrate() const;


    // ========================================================
    // Controller communication
    // ========================================================

    bool openSelectedController();

    bool openController(
        const char* port,
        const char* controllerType,
        int baudrate
    );

    bool execute(
        const MachineCommand& command
    );


    // ========================================================
    // G-code
    // ========================================================

    bool sendGcode(
        const char* gcode
    );

    bool sendGcode(
        const char* port,
        const char* gcode
    );


    // ========================================================
    // CNCjs controller commands
    // ========================================================

    bool sendCommand(
        const String& command
    );

    bool jogCancel();

    bool feedHold();

    bool resume();

    bool reset();

    bool sendRealtime(
        uint8_t command
    );


private:

    // ========================================================
    // CNCjs server
    // ========================================================

    String serverHost_;

    uint16_t serverPort_ =
        8000;

    IPAddress serverIP_;


    void loadServerSettings();

    void saveServerSettings(
        const char* host,
        uint16_t port
    );


    // ========================================================
    // Network
    // ========================================================

    NetworkManager networkManager_;


    // ========================================================
    // Connection state machine
    // ========================================================

    enum class ConnectionState
    {
        Start,
        WiFiConnecting,
        Resolving,
        Authenticating,
        SocketConnecting,
        WaitingForLists,
        OpeningController,
        Ready,
        Backoff
    };


    ConnectionState connectionState =
        ConnectionState::Start;

    CNCjsStatus currentStatus_ =
        CNCjsStatus::Offline;

    unsigned long stateStartedAt =
        0;

    unsigned long retryAt =
        0;


    static constexpr unsigned long WIFI_TIMEOUT =
        15000;

    static constexpr unsigned long RESOLVE_TIMEOUT =
        5000;

    static constexpr unsigned long AUTH_TIMEOUT =
        5000;

    static constexpr unsigned long SOCKET_TIMEOUT =
        10000;

    static constexpr unsigned long RECONNECT_DELAY =
        3000;


    void updateConnection();

    void enterConnectionState(
        ConnectionState state
    );

    void connectionFailed(
        const char* reason
    );


    // ========================================================
    // Authentication token
    // ========================================================

    String token;


    // ========================================================
    // Socket.IO
    // ========================================================

    SocketIOclient socketIO;

    bool socketBeginRequested =
        false;


    void connectSocket();

    void handleSocketEvent(
        socketIOmessageType_t type,
        uint8_t* payload,
        size_t length
    );


    static CNCjsClientCore* instance;


    static void socketIOEvent(
        socketIOmessageType_t type,
        uint8_t* payload,
        size_t length
    );


    // ========================================================
    // Connection state
    // ========================================================

    bool authenticatedState =
        false;

    bool socketConnectedState =
        false;


    // ========================================================
    // Machine state
    // ========================================================

    MachineState* machineState_ =
        nullptr;


    void updateMachineState(
        JsonObject status,
        JsonObject parserstate
    );


    bool parseGrblStatusReport(
        const char* report
    );


    MachineStatus machineStatusFromCNCjs(
        const char* activeState
    ) const;


    // ========================================================
    // Machine heartbeat
    // ========================================================

    static constexpr unsigned long HEARTBEAT_INTERVAL =
        1000;

    static constexpr unsigned long HEARTBEAT_TIMEOUT =
        5000;


    unsigned long lastHeartbeatTime =
        0;

    unsigned long lastMachineStateTime =
        0;


    bool heartbeatWaiting =
        false;


    void updateHeartbeat();

    bool sendStatusReport();

    void machineHeartbeatReceived();


    // ========================================================
    // Pending machine command
    // ========================================================

    MachineCommand pendingCommand_;

    bool commandDirty_ =
        false;

    unsigned long lastCommandSendTime_ =
        0;


    static constexpr unsigned long COMMAND_SEND_INTERVAL =
        50;


    bool sendPendingCommand();


    // ========================================================
    // Serial ports
    // ========================================================

    static constexpr int MAX_PORTS =
        8;


    String ports[MAX_PORTS];

    int numberOfPorts =
        0;

    bool listRequested =
        false;


    // ========================================================
    // Controllers
    // ========================================================

    static constexpr int MAX_CONTROLLERS =
        8;


    String controllers[MAX_CONTROLLERS];

    int numberOfControllers =
        0;


    // ========================================================
    // Lists received
    // ========================================================

    bool startupReceivedState =
        false;

    bool portListReceivedState =
        false;


    // ========================================================
    // Selected controller
    // ========================================================

    int selectedControllerIndex =
        -1;

    String selectedControllerNameState;

    int selectedPortIndex =
        -1;

    String selectedPortNameState;

    bool controllerSelectionReadyState =
        false;


    // ========================================================
    // Active controller
    // ========================================================

    String activeControllerPortState;

    String activeControllerTypeState;

    int activeControllerBaudrateState =
        0;

    bool controllerReadyState =
        false;


    // ========================================================
    // Controller selection
    // ========================================================

    void requestPortList();

    void loadControllerList();

    int loadSavedController();

    String loadSavedControllerName();

    String loadSavedPortName();


    void saveSelectedController(
        int index,
        const char* name
    );

    void saveSelectedPort(
        const char* port
    );


    int findControllerByName(
        const char* name
    ) const;

    int findPortByName(
        const char* name
    ) const;


    int baudrateForController(
        const char* controllerType
    ) const;
};


#endif
