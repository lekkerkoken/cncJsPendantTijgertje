#ifndef CNCJS_CLIENT_CORE_H
#define CNCJS_CLIENT_CORE_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <SocketIOclient.h>
#include <ArduinoJson.h>

#include "MachineCommand.h"
#include "MachineState.h"
#include "NetworkManager.h"
#include "ControllerSettingsSnapshot.h"
#include "ControllerStateSnapshot.h"
#include "Config.h"


class CNCjsClientCore
{
public:

    // ========================================================
    // STATUS
    // ========================================================

    enum class CNCjsStatus
    {
        Offline,
        WiFiConnecting,
        Authenticating,
        Connecting,
        WaitingForLists,
        ControllerSelectionPending,
        OpeningController,
        Ready,
        Error
    };


    // ========================================================
    // SNAPSHOT
    // ========================================================

    struct CNCjsSnapshot
    {
        CNCjsStatus status;

        bool wifiConnected;
        bool authenticated;
        bool socketConnected;

        int portCount;
        int controllerCount;

        int selectedController;
        String selectedControllerName;

        int selectedPort;
        String selectedPortName;

        bool controllerSelectionReady;

        bool controllerReady;

        String controllerPort;
        String controllerType;
        int controllerBaudrate;


        CNCjsSnapshot()
            :
            status(CNCjsStatus::Offline),
            wifiConnected(false),
            authenticated(false),
            socketConnected(false),
            portCount(0),
            controllerCount(0),
            selectedController(-1),
            selectedControllerName(""),
            selectedPort(-1),
            selectedPortName(""),
            controllerSelectionReady(false),
            controllerReady(false),
            controllerPort(""),
            controllerType(""),
            controllerBaudrate(0)
        {
        }
    };


    // ========================================================
    // THREADING CONTRACT
    // ========================================================
    //
    // commando → thread-safe naar Core
    // state     → snapshot uit Core
    //
    // CNCjsClientCore bezit zijn eigen MachineState.
    //
    // Publieke commando-methodes zijn thread-safe.
    //
    // State wordt uitsluitend als snapshot naar buiten gegeven.
    //
    // ========================================================


    // ========================================================
    // LIFECYCLE
    // ========================================================

    void begin();

    void update();


    // ========================================================
    // SNAPSHOTS
    // ========================================================

    CNCjsSnapshot snapshot() const;

    MachineState machineStateSnapshot() const;

    ControllerStateSnapshot
    controllerStateSnapshot() const;

    ControllerSettingsSnapshot
    controllerSettingsSnapshot() const;

    // Kept for compatibility with existing code.
    MachineState getMachineState() const;


    // ========================================================
    // STATUS
    // ========================================================

    CNCjsStatus status() const;


    // ========================================================
    // CONNECTION
    // ========================================================

    bool wifiConnected() const;

    bool authenticated() const;

    bool socketConnected() const;


    // ========================================================
    // SERIAL PORTS
    // ========================================================

    int portCount() const;

    String port(
        int index
    ) const;


    // ========================================================
    // CONTROLLERS
    // ========================================================

    int controllerCount() const;

    String controller(
        int index
    ) const;


    // ========================================================
    // CONTROLLER SELECTION
    // ========================================================

    bool controllerSelectionReady() const;

    int selectedController() const;

    String selectedControllerName() const;

    int selectedPort() const;

    String selectedPortName() const;


    bool selectController(
        int portIndex,
        int controllerIndex
    );

    void chooseController();


    // ========================================================
    // ACTIVE CONTROLLER
    // ========================================================

    bool controllerReady() const;

    String controllerPort() const;

    String controllerType() const;

    int controllerBaudrate() const;


    // ========================================================
    // CONTROLLER COMMUNICATION
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
    // G-CODE
    // ========================================================

    bool sendGcode(
        const char* gcode
    );

    bool sendGcode(
        const char* port,
        const char* gcode
    );


    // ========================================================
    // CNCjs CONTROLLER COMMANDS
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
    // SYNCHRONIZATION
    // ========================================================

    mutable SemaphoreHandle_t networkMutex_ =
        nullptr;


    bool lock() const;

    void unlock() const;


    // ========================================================
    // FREERTOS NETWORK TASK
    // ========================================================

    static constexpr uint32_t NETWORK_TASK_STACK_SIZE =
        8192;

    static constexpr UBaseType_t NETWORK_TASK_PRIORITY =
        1;

    static constexpr uint32_t NETWORK_TASK_DELAY_MS =
        5;


    TaskHandle_t networkTaskHandle_ =
        nullptr;


    static void networkTaskEntry(
        void* parameter
    );

    void networkTask();


    // ========================================================
    // CNCjs SERVER
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
    // NETWORK
    // ========================================================

    NetworkManager networkManager_;


    // ========================================================
    // CONNECTION STATE MACHINE
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
    // AUTHENTICATION TOKEN
    // ========================================================

    String token;


    // ========================================================
    // SOCKET.IO
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
    // CONNECTION FLAGS
    // ========================================================

    bool authenticatedState =
        false;

    bool socketConnectedState =
        false;


    // ========================================================
    // MACHINE STATE
    // ========================================================

    MachineState machineState_;

    ControllerStateSnapshot controllerState_;
    ControllerSettingsSnapshot controllerSettings_;

    void updateMachineState(
        JsonObject status,
        JsonObject parserstate
    );


    MachineStatus machineStatusFromCNCjs(
        const char* activeState
    ) const;


    // ========================================================
    // MACHINE HEARTBEAT / ACTIVITY WATCHDOG
    // ========================================================
    //
    // lastCncjsActivity:
    //   Laatste ontvangen CNCjs EVENT.
    //
    // lastHeartbeatPing:
    //   Laatste verstuurde heartbeat-ping.
    //
    // De watchdog en de heartbeat-rate zijn bewust
    // onafhankelijk van elkaar.
    //
    // ========================================================

    static constexpr unsigned long CNCJS_ACTIVITY_TIMEOUT =
        3000;

    static constexpr unsigned long HEARTBEAT_INTERVAL =
        3000;
        
    static constexpr unsigned long HEARTBEAT_TIMEOUT =
        5000;



    unsigned long lastCncjsActivity =
        0;

    unsigned long lastMachineStateTime =
        0;

    unsigned long lastHeartbeatPing =
        0;


    void updateHeartbeat();

    bool heartbeatPing();

    void cncjsActivityReceived();

    void machineHeartbeatReceived();


    // ========================================================
    // PENDING MACHINE COMMAND
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
    // SERIAL PORTS
    // ========================================================

    static constexpr int MAX_PORTS =
        8;

    String ports[MAX_PORTS];

    int numberOfPorts =
        0;

    bool listRequested =
        false;


    // ========================================================
    // CONTROLLERS
    // ========================================================

    static constexpr int MAX_CONTROLLERS =
        8;

    String controllers[MAX_CONTROLLERS];

    int numberOfControllers =
        0;


    // ========================================================
    // LISTS RECEIVED
    // ========================================================

    bool startupReceivedState =
        false;

    bool portListReceivedState =
        false;

    bool controllerListProcessedState =
        false;


    // ========================================================
    // SELECTED CONTROLLER
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
    // ACTIVE CONTROLLER
    // ========================================================

    String activeControllerPortState;

    String activeControllerTypeState;

    int activeControllerBaudrateState =
        0;

    bool controllerReadyState =
        false;


    // ========================================================
    // CONTROLLER SELECTION
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


    // ========================================================
    // INTERNAL CONTROLLER OPERATIONS
    // ========================================================

    bool openSelectedControllerInternal();

    bool openControllerInternal(
        const char* port,
        const char* controllerType,
        int baudrate
    );

    bool sendGcodeInternal(
        const char* port,
        const char* gcode
    );

    bool sendCommandInternal(
        const String& command
    );

    bool sendRealtimeInternal(
        uint8_t command
    );

    bool jogCancelInternal();

    bool feedHoldInternal();

    bool resumeInternal();

    bool resetInternal();
};


#endif