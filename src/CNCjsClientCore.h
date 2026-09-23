#ifndef CNCJS_CLIENT_CORE_H
#define CNCJS_CLIENT_CORE_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <SocketIOclient.h>
#include <ArduinoJson.h>
#include <functional>

#include "NetworkManager.h"
#include "ControllerSettingsSnapshot.h"
#include "ControllerStateSnapshot.h"
#include "SenderStatusSnapshot.h"
#include "JobSnapshot.h"
#include "LatestStateTripleBuffer.h"

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

        bool controllerSettingsReadyState;

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
    // Publieke commando-methodes zijn thread-safe.
    //
    //
    // ========================================================


    // ========================================================
    // LIFECYCLE
    // ========================================================

    void begin();


    // ========================================================
    // SNAPSHOTS
    // ========================================================

    CNCjsSnapshot snapshot() const;

    ControllerStateSnapshot
    controllerStateSnapshot() const;

    ControllerSettingsSnapshot
    controllerSettingsSnapshot() const;

    uint32_t
    controllerSettingsPublicationId() const
    {
        return controllerSettingsPublicationId_;
    }

    void confirmControllerSettingsPublished();

    SenderStatusSnapshot
    senderStatusSnapshot() const;

    JobSnapshot
    jobSnapshot() const;


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

    void updateControllerReadyState();

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

    bool fetchMacros(JsonDocument& document);

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
        const String& command,
        const JsonObjectConst& options = JsonObjectConst()
    );


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

    bool fetchMacrosInternal(JsonDocument& document);


    // ========================================================
    // DEBUG / EVENT STREAM INSTRUMENTATION
    // ========================================================

    struct EventDebugCounters
    {
        uint32_t networkTaskCalls = 0;
        uint32_t socketLoopCalls = 0;

        uint32_t controllerStateEvents = 0;
        uint32_t grblStateEvents = 0;
        uint32_t serialportReadEvents = 0;
        uint32_t serialportWriteEvents = 0;
        uint32_t otherEvents = 0;

        uint32_t socketLoopTotalUs = 0;
        uint32_t socketLoopMaxUs = 0;

        uint32_t eventTotal = 0;
    };


    EventDebugCounters eventDebug_;

    unsigned long eventDebugWindowStartedAt = 0;


    void debugEvent(
        const char* eventName,
        size_t length
    );


    void debugStateEvent(
        const char* eventName,
        JsonArray array
    );


    void debugSerialportWrite(
        JsonArray array
    );


    void debugSerialportRead(
        JsonArray array
    );


    void debugReport();


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
        WaitingForControllerSettingsPublication,
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

    bool reopenActiveController();

    bool authenticatedState =
        false;

    bool socketConnectedState =
        false;


    // ========================================================
    // LATEST STATE
    // ========================================================

    LatestStateTripleBuffer<ControllerStateSnapshot>
        controllerStateBuffer_;

    LatestStateTripleBuffer<ControllerSettingsSnapshot>
        controllerSettingsBuffer_;

    LatestStateTripleBuffer<SenderStatusSnapshot>
        SenderStatusBuffer_;

    LatestStateTripleBuffer<JobSnapshot>
        jobBuffer_;

    uint32_t controllerSettingsPublicationId_;


    void invalidateControllerState();

    void invalidateControllerSettings();

    void invalidateSenderStatus();

    void invalidateJob();

    // ========================================================
    // MACHINE HEARTBEAT / ACTIVITY WATCHDOG
    // ========================================================
    //
    // lastCncjsActivity:
    //   Laatste ontvangen controller:state.
    //
    // TOLERABLE_SILENCE_DURATION:
    //   Hoe lang we geen controller:state willen missen
    //   voordat we actief om een statusreport vragen.
    //
    // lastHeartbeatPing:
    //   Laatste heartbeat-ping.
    //
    // HEARTBEAT_TIMEOUT:
    //   Onafhankelijke watchdog voor machine-state.
    //
    // ========================================================

    static constexpr unsigned long TOLERABLE_SILENCE_DURATION =
        3000;

    static constexpr unsigned long HEARTBEAT_TIMEOUT =
        5000;


    static constexpr unsigned long CONTROLLER_SETTINGS_RETRY_INTERVAL = 2000;

    static constexpr uint8_t CONTROLLER_SETTINGS_MAX_RETRIES = 3;

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
    // SERIAL PORT READ HANDLER
    // ========================================================

    std::function<void(const JsonArray&)> serialportReadHandler_;

    bool controllerVerifiedThisSession_ =
        false;

    bool controllerCorrectionAttemptedThisSession_ =
        false;


    void handleControllerIdentification(
        const JsonArray& array
    );

    void handleSerialportRead(
        const JsonArray& array
    );

    String identifyControllerFromStatusReport(
        const JsonArray& array
    ) const;



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

    bool controllerSettingsReadyState = false;

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
        const String& command,
        const JsonObjectConst& options
    );

};


#endif