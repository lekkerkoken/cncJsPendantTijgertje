READMEold.md

## Controller-specifieke aansturing

Verschillende CNC-controllers gebruiken verschillende commando's en protocollen om dezelfde machinehandeling uit te voeren. Een jogcommando voor GRBL is bijvoorbeeld niet zonder meer hetzelfde als een jogcommando voor TinyG.

Daarom wordt de **controller-specifieke aanstuurcode in de pendant zelf opgebouwd**.

De `MachineMapper` vormt hierbij de vertaallaag tussen het abstracte machinecommando van de pendant en de concrete aansturing voor de geselecteerde controller.

Bijvoorbeeld:

```text
JogPlanner

    │

    │  JogCommand

    ▼

MachineMapper

    │

    ├── GRBL  → GRBL-specifieke aansturing

    │

    └── TinyG → TinyG-specifieke aansturing

    │

    ▼

CNCjsInterface

    │

    ▼

CNCjs
```

CNCjs blijft daarbij de communicatiegrens tussen de pendant en de fysieke controller. De pendant hoeft dus niet rechtstreeks met de seriële verbinding van de CNC te communiceren.

### Uitzondering: jog cancel

Voor het annuleren van een actieve jog ligt dit anders.

CNCjs beschikt zelf over een `jogCancel`-commando en bevat daarvoor controller-specifieke implementaties. Zo weet CNCjs zelf hoe een jog bij bijvoorbeeld GRBL of TinyG moet worden geannuleerd.

Daarom gebruiken we voor `JOG_CANCEL` bewust de bestaande CNCjs-functionaliteit in plaats van deze opnieuw in de pendant te implementeren.

```text
JOG_CANCEL

    │

    ▼

CNCjsInterface

    │

    │  CNCjs jogCancel

    ▼

CNCjs

    │

    ├── GRBL → controller-specifieke cancel

    └── TinyG → controller-specifieke cancel
```

De ontwerpregel is daarmee:

> **Controller-specifieke machineaansturing die CNCjs niet voor ons abstraheert, implementeren we in `MachineMapper`. Functionaliteit die CNCjs zelf al controller-specifiek aanbiedt, hergebruiken we.**

Hiermee blijft de pendant onafhankelijk van de precieze CNCjs-implementatie, zonder dat we controller-specifieke aansturing onnodig dubbel implementeren.

## PendantController

`PendantController` is de centrale bedieningslaag van de CNC-pendant.

De `PendantController` vertaalt gebeurtenissen uit de fysieke bediening (`Event`) naar veranderingen in de pendanttoestand (`PendantState`) en bepaalt op basis van de pendant-, machine- en CNCjs-toestand wat er op het display wordt weergegeven.

De `PendantController` bevat **geen implementatie van WiFi, Socket.IO, CNCjs-communicatie of machinecommunicatie**. Die verantwoordelijkheden blijven bij de daarvoor bestemde componenten.

### Verantwoordelijkheden

`PendantController` is verantwoordelijk voor:

* verwerken van `Event`-objecten;
* wijzigen van de `PendantState`;
* bepalen welke pendantlaag actief is;
* bepalen welke as en jog-instelling actief zijn;
* reageren op de toestand van `MachineState`;
* reageren op de toestand van `CNCjsInterface`;
* bepalen welke informatie op het display prioriteit heeft;
* aansturen van `Display`.

De controller is daarmee het centrale punt voor **pendantgedrag**, maar niet voor de technische implementatie van de onderliggende subsystemen.

### Relatie met andere componenten

De architectuur is als volgt:

```text
InputManager

     │

     │ Event

     ▼

PendantController

     │

     ├──────────────► PendantState

     │

     ├──────────────► Display

     │

     ├──────────────► MachineState

     │

     └──────────────► CNCjsInterface
```

De afzonderlijke verantwoordelijkheden blijven gescheiden:

| Component                | Verantwoordelijkheid                                     |
| ------------------------ | -------------------------------------------------------- |
| `InputManager`           | Fysieke inputs vertalen naar `Event`                     |
| `PendantController`      | Pendantgedrag en displayprioriteit                       |
| `PendantState`           | Toestand van de bediening                                |
| `Display`                | Weergeven van informatie                                 |
| `MachineState`           | Toestand van de CNC-machine                              |
| `CNCjsInterface`            | WiFi, mDNS, authenticatie, Socket.IO en CNCjs-controller |
| `JogPlanner`             | Plannen van jogbewegingen                                |
| `MachineMapper`          | Vertalen tussen machine- en pendantcoördinaten           |
| `PowerManager` *(later)* | Sleep-, wake- en energiebeheer                           |

## Inputarchitectuur en asynchrone verwerking

De fysieke inputs van de pendant moeten onafhankelijk kunnen functioneren van netwerkcommunicatie en andere applicatietaken.

De ESP32-S3 gebruikt hiervoor FreeRTOS-taken.

De belangrijkste hardware- en communicatietaken zijn:

```text
                    ESP32-S3

                       │

        ┌──────────────┼──────────────┐
        │              │              │
        ▼              ▼              ▼

   Encoder Task   ButtonMatrix     CNCjs Task
                     Task
        │              │              │
        └──────┬───────┘              │
               │                      │
               ▼                      │
          Input Queue                 │
               │                      │
               ▼                      │
        InputManager                  │
               │                      │
               ▼                      │
       Application logic              │
               │                      │
               ▼                      │
          JogPlanner                  │
               │                      │
               ▼                      │
        CNCjs command queue           │
                                      │
                                      ▼
                                CNCjs / Socket.IO
```

Het doel hiervan is dat een tijdelijke vertraging in bijvoorbeeld WiFi, Socket.IO of CNCjs de encoder niet kan blokkeren.

### Encoder

De `Encoder` is verantwoordelijk voor het interpreteren van de fysieke encoderhardware.

De encoder verwerkt:

* A/B quadrature-signalen;
* draairichting;
* encoderdruk;
* debounce van de encoderknop.

De encoder rapporteert intern:

```text
ENCODER_PULSE
ENCODER_PRESS
```

Een `ENCODER_PULSE` bevat een richting:

```text
+1 = rechts

-1 = links
```

De `Encoder` kent geen `Event` uit de applicatielaag. Hij rapporteert een eigen `EncoderEvent`.

De `InputManager` vertaalt dit vervolgens naar het algemene `Event`-model:

```text
Encoder

    │

    │ EncoderEvent

    ▼

InputManager

    │

    │ Event

    ├── EVENT_ENCODER_PULSE, +1
    ├── EVENT_ENCODER_PULSE, -1
    └── EVENT_ENCODER_PRESS
```

### ButtonMatrix

De `ButtonMatrix` is verantwoordelijk voor het uitlezen van de fysieke knopmatrix.

De pinconfiguratie behoort tot `Config.h`, zodat de matrixhardware niet hardcoded in de class zit.

De `InputManager` verzorgt de debounce en vertaalt een stabiele toetsdruk naar een `Event`.

Bijvoorbeeld:

```text
ButtonMatrix

    │

    │ key 3

    ▼

InputManager

    │

    ▼

EVENT_KEY_3
```

### Encoder heeft voorrang

Wanneer meerdere inputbronnen vrijwel gelijktijdig input leveren, krijgt de encoder voorrang boven matrixknoppen.

De reden is dat encoderinput gebruikt kan worden voor continue bediening, zoals joggen en het aanpassen van jog-instellingen.

De prioriteit binnen `InputManager` is daarom:

```text
1. Encoder

2. ButtonMatrix
```

Per verwerking wordt maximaal één logisch `Event` aan de applicatielaag aangeboden.

### Encoderpulsen coalescen

De encoder kan sneller input genereren dan de hogere applicatielagen hoeven te verwerken.

Daarom worden opeenvolgende encoderpulsen in `InputManager` samengevoegd wanneer deze nog beschikbaar zijn voordat het event aan de volgende laag wordt aangeboden.

Bijvoorbeeld:

```text
Encoder:

    +1
    +1
    +1
    +1

        ↓

InputManager:

    EVENT_ENCODER_PULSE
    value = +4
```

Hierdoor hoeft `JogPlanner` niet vier afzonderlijke events te verwerken.

Bij een richtingsverandering blijft de volgorde van verschillende richtingen behouden.

Bijvoorbeeld:

```text
+1
+1
+1
-1

        ↓

EVENT_ENCODER_PULSE
value = +3

EVENT_ENCODER_PULSE
value = -1
```

De coalescing gebeurt bewust in `InputManager`.

`JogPlanner` hoeft daardoor niets te weten over de fysieke encoderfrequentie of over de manier waarop individuele encoderpulsen worden gelezen.

De verantwoordelijkheid is:

```text
Encoder

    ↓

fysieke encoderpulsen

    ↓

InputManager

    ↓

coalescen en normaliseren

    ↓

Event

    ↓

JogPlanner
```

### Waarom coalescing in InputManager?

`Encoder` kent alleen de fysieke hardware.

`JogPlanner` kent alleen de gebruikersintentie.

`InputManager` vormt de overgang tussen die twee werelden en is daarom de juiste plaats om meerdere fysieke inputtransities samen te voegen tot één betekenisvol gebruikers-event.

De `JogPlanner` ontvangt daardoor bijvoorbeeld:

```text
"de gebruiker wil 4 stappen naar rechts"
```

in plaats van:

```text
"er kwamen vier afzonderlijke elektrische encodertransities binnen".
```

Hierdoor blijft de `JogPlanner` volledig onafhankelijk van de gebruikte encoderhardware.

## Asynchrone taakverdeling

De ESP32-S3 gebruikt FreeRTOS om verschillende onderdelen onafhankelijk van elkaar te laten functioneren.

De globale taakverdeling is:

```text
┌───────────────────────────────────────────────┐
│                   ESP32-S3                    │
│                                               │
│  Encoder Task ──────┐                         │
│                     │                         │
│  Button Task ───────┼──> Input Queue          │
│                     │         │               │
│                     │         ▼               │
│                     │   Application Task      │
│                     │      │       │          │
│                     │      ▼       ▼          │
│                     │   JogPlanner Controller │
│                     │      │                  │
│                     │      ▼                  │
│                     │  CNCjs Command Queue    │
│                     │      │                  │
│                     │      ▼                  │
│                     └─ CNCjs Task             │
│                            │                  │
│                            ▼                  │
│                         Socket.IO             │
└───────────────────────────────────────────────┘
```

De exacte FreeRTOS-prioriteiten en intervallen worden later bepaald op basis van de daadwerkelijke belasting.

Het ontwerpprincipe staat echter vast:

> **Een trage of tijdelijk geblokkeerde subsystemen mag de tijdkritische inputverwerking niet blokkeren.**

Dit is met name belangrijk voor de encoder.

### Encoderfrequentie

De encoder kan bij snel draaien tientallen pulsen per seconde produceren.

De encoder-task moet daarom snel genoeg worden uitgevoerd om geen quadrature-transities te missen.

De hogere applicatielaag hoeft deze frequentie niet te volgen. `InputManager` normaliseert de input en coalescet beschikbare pulsen.

Daarmee kunnen bijvoorbeeld:

```text
Encoder Task

    +1 +1 +1 +1 +1 +1

             ↓

InputManager

    +6

             ↓

JogPlanner
```

worden verwerkt zonder dat `JogPlanner` zes keer hoeft te worden aangeroepen.

## Displayprioriteit

Het display kent verschillende bronnen van informatie. De actieve pendantlaag is daarom niet altijd de hoogste prioriteit.

Machine- en verbindingsstatus kunnen de normale pendantweergave overschrijven.

De huidige prioriteitsgedachte is:

```text
Machine ALARM / HOLD

        │

        ▼

CNCjs / verbindingstoestand

        │

        ▼

Normale pendantweergave

        │

        ├── JOG

        ├── INFO

        └── CONTROL
```

### Machine-status

Een actieve machinewaarschuwing zoals `ALARM` of `HOLD` heeft de hoogste prioriteit.

Dit betekent dat bijvoorbeeld de normale `JOG`, `INFO` of `CONTROL`-weergave tijdelijk plaatsmaakt voor de machine-status.

Dit voorkomt dat belangrijke machine-informatie verborgen wordt door de normale pendantinterface.

### CNCjs-verbindingsstatus

Ook de verbinding met CNCjs mag de normale pendantweergave beïnvloeden.

Bijvoorbeeld tijdens het opstarten:

```text
WiFi connecting...

CNCjs authenticating...

CNCjs connecting...

Controller available

Controller opening...

Ready
```

De precieze teksten en schermindeling kunnen later worden bepaald, maar het uitgangspunt is dat de gebruiker altijd kan zien wanneer de pendant nog geen bruikbare CNCjs-verbinding heeft.

Een verbindingsstatus heeft echter niet automatisch dezelfde prioriteit als een machine-`ALARM` of `HOLD`.

Bijvoorbeeld:

```text
Machine ALARM

    ↓

altijd ALARM tonen
```

ook wanneer tegelijkertijd:

```text
CNCjs reconnecting...
```

Dit voorkomt dat een verbindingsmelding een belangrijkere machinewaarschuwing verdringt.

## Pendantlagen

De pendant kent momenteel drie lagen:

```cpp
LAYER_JOG
LAYER_INFO
LAYER_COMMAND
```

Deze lagen beschrijven de **normale bediening** van de pendant.

Ze zijn dus niet hetzelfde als een verbindings- of machine-status.

Een pendant kan bijvoorbeeld:

```text
LAYER_JOG

+

CNCjs = Ready

+

Machine = IDLE
```

zijn, maar ook:

```text
LAYER_JOG

+

CNCjs = Connecting

+

Machine = onbekend
```

De actieve laag blijft in dat geval `LAYER_JOG`; alleen de displayweergave kan tijdelijk worden overschreven door de verbindingsstatus.

## Verbinding met CNCjs

De `CNCjsInterface` beheert de volledige technische verbinding met CNCjs.

De pendant hoeft niet te weten hoe die verbinding tot stand komt.

De client verzorgt onder andere:

```text
WiFi

  ↓

mDNS (cncjs.local)

  ↓

HTTP authentication

  ↓

Socket.IO

  ↓

controller/port discovery

  ↓

saved controller selection

  ↓

controller openen

  ↓

Ready
```

De pendant reageert uitsluitend op de resulterende status.

Bij verbindingsverlies kan `CNCjsInterface` zelfstandig opnieuw proberen verbinding te maken. Zodra de status verandert, kan `PendantController` de displayweergave daarop aanpassen.

Na deep sleep wordt dezelfde verbindingsprocedure opnieuw uitgevoerd; een oude Socket.IO-verbinding wordt niet hergebruikt.

## Tijdelijke startup-bediening

Tijdens de ontwikkeling wordt `EVENT_KEY_9` tijdelijk gebruikt om de voorgestelde CNCjs-controller te openen.

Conceptueel:

```text
CNCjs verbonden

      ↓

saved controller gevonden

      ↓

SelectionRequired

      ↓

KEY 9

      ↓

openSelectedController()

      ↓

OpeningController

      ↓

Ready
```

Deze functie is tijdelijk bedoeld als ontwikkel- en testmechanisme.

Uiteindelijk kan dezelfde bediening worden geïntegreerd in de normale `CONTROL`-laag van de pendant.

## Sleep en reconnect

Slaapgedrag behoort niet rechtstreeks tot `PendantController`.

De toekomstige verdeling is:

```text
PendantController

    │

    │ bepaalt dat slapen gewenst is

    ▼

PowerManager

    │

    ├── display dimmen

    ├── light sleep

    ├── deep sleep

    └── wake-up configuratie
```

Bij deep sleep wordt de CNCjs-verbinding verbroken. Na wake-up wordt de normale verbindingsprocedure opnieuw uitgevoerd.

Het doel is dat de gebruiker na een slaap/wake-cyclus niet handmatig opnieuw hoeft te configureren:

```text
wake

 ↓

WiFi

 ↓

cncjs.local

 ↓

authenticate

 ↓

Socket.IO

 ↓

saved controller

 ↓

controller openen

 ↓

Ready
```

De opgeslagen CNCjs-host, poort en controllerselectie blijven hiervoor in NVS beschikbaar.

## Ontwerpprincipe

De belangrijkste ontwerpregel voor `PendantController` is:

> **De PendantController bepaalt wat de gebruiker ziet en hoe de bediening reageert; hij implementeert niet zelf de onderliggende techniek.**

Daarnaast geldt voor de inputarchitectuur:

> **Fysieke input wordt zo vroeg mogelijk onafhankelijk gemaakt van de rest van de applicatie. Encoder- en matrixinput mogen niet worden geblokkeerd door netwerk- of machinecommunicatie.**

---

# CNCjsInterface – communicatie- en machine-selectiecontract

## Doel

De `CNCjsInterface` vormt de technische communicatiegrens tussen de pendant en CNCjs.

De rest van de pendant hoeft niets te weten over:

* HTTP
* JWT
* Socket.IO
* Engine.IO
* CNCjs API endpoints
* CNCjs eventnamen
* JSON-serialisatie
* seriële poorten
* CNCjs controllerdrivers

De hogere lagen werken uitsluitend met de eigen modellen van de pendant, zoals:

```text
MachineCommand

MachineState
```

---

## 15. CNCjsInterface als adapterlaag

De CNCjsInterface heeft twee hoofdverantwoordelijkheden:

1. communicatie met CNCjs verzorgen;
2. een geschikte CNCjs-machineverbinding vinden en openen.

De CNCjsInterface vertaalt dus niet de gebruikersintentie.

De architectuur blijft:

```text
JogPlanner

    ↓

MachineCommand

    ↓

MachineMapper

    ↓

CNCjsInterface

    ↓

CNCjs

    ↓

GRBL / TinyG
```

Machinefeedback loopt terug:

```text
GRBL / TinyG

    ↓

CNCjs

    ↓

CNCjsInterface

    ↓

MachineState

    ↓

JogPlanner
```

---

## 16. Authenticatie

Ook wanneer CNCjs gebruikerslogin heeft uitgeschakeld, gebruikt CNCjs authenticatie voor de API en Socket.IO-interface.

De CNCjsInterface vraagt daarom bij het opstarten een JWT-token op via:

```text
POST /api/signin
```

met:

```text
{"token":""}
```

Bij een installatie zonder actieve gebruikerslogin levert CNCjs hiermee een geldig token voor de client op.

De gebruiker hoeft dus niet interactief in te loggen.

De volgorde is:

```text
HTTP POST /api/signin

         ↓

     JWT token

         ↓

      Socket.IO
```

---

## 17. Socket.IO

De huidige CNCjs-installatie gebruikt:

```text
Socket.IO

Engine.IO versie 3
```

De verbinding wordt daarom geopend via:

```text
/socket.io/?token=<JWT>&EIO=3
```

De ESP32 gebruikt hiervoor:

```text
Links2004/arduinoWebSockets
```

en de daarin beschikbare:

```text
SocketIOclient
```

De CNCjsInterface verbergt deze implementatiedetails voor de rest van de pendant.

---

## 18. CNCjs-inventarisatie

Na het opbouwen van de Socket.IO-verbinding wacht de CNCjsInterface op informatie van CNCjs over de beschikbare controllers en seriële poorten.

CNCjs levert onder andere een `startup` event:

```text
["startup", {
    "loadedControllers": [
        "Grbl",
        "Marlin",
        "Smoothie",
        "TinyG"
    ],
    ...
}]
```

Dit is de lijst van controllerdrivers die de huidige CNCjs-installatie ondersteunt.

Daarnaast levert CNCjs:

```text
serialport:list
```

Bijvoorbeeld:

```text
["serialport:list", [
    {
        "port": "/dev/ttyUSB0",
        "manufacturer": "1a86",
        "inuse": false
    }
]]
```

Deze twee gegevens hebben verschillende betekenissen.

`loadedControllers` betekent:

```text
welke controllerdrivers CNCjs kent.
```

`serialport:list` betekent:

```text
welke seriële apparaten CNCjs momenteel ziet.
```

De index van deze twee lijsten is daarom niet hetzelfde.

We mogen dus niet aannemen:

```text
loadedControllers[0] == serialport:list[0]
```

De CNCjsInterface combineert deze informatie tot mogelijke machineconfiguraties.

---

## 19. Beschikbare machines

De CNCjsInterface bouwt uit de ontvangen informatie een lijst van mogelijke machineverbindingen.

Conceptueel:

```text
AvailableMachine

    controllerType

    port

    baudrate

    manufacturer

    overige relevante eigenschappen
```

Bijvoorbeeld:

```text
1. Grbl  /dev/ttyUSB0  115200
2. TinyG /dev/ttyUSB1  115200
```

Deze lijst vormt de basis voor de machinekeuze.

---

## 20. Controllerkeuze

De CNCjsInterface onthoudt welke machine eerder door de gebruiker is gekozen.

De opgeslagen keuze is **geen index**.

We slaan bijvoorbeeld op:

```text
controllerType = Grbl

port           = /dev/ttyUSB0

baudrate       = 115200

manufacturer   = 1a86
```

Een index is namelijk alleen betekenisvol binnen de huidige lijst.

Bij een volgende start kan bijvoorbeeld:

```text
/dev/ttyUSB0
```

zijn veranderd in:

```text
/dev/ttyUSB1
```

De opgeslagen eigenschappen blijven dan bruikbaar om de eerder gekozen machine terug te vinden.

---

## 21. Automatische selectie

Na inventarisatie vergelijkt de CNCjsInterface de eerder opgeslagen keuze met de actuele lijst.

Wanneer exact één geschikte match wordt gevonden:

```text
opgeslagen keuze

      ↓

gevonden in actuele lijst

      ↓

automatisch openen
```

De gebruiker hoeft dan niets te doen.

---

## 22. Wanneer de vorige keuze niet beschikbaar is

Wanneer de opgeslagen machine niet meer wordt gevonden, wordt niet automatisch een willekeurige machine geopend.

De CNCjsInterface gaat naar:

```text
MACHINE_SELECTION_REQUIRED
```

Bijvoorbeeld:

```text
Beschikbare machines:

1. Grbl  /dev/ttyUSB0

2. TinyG /dev/ttyUSB1
```

De gebruiker kan dan een machine kiezen.

Voorlopig kan deze keuze bijvoorbeeld rechtstreeks via de pendant worden gemaakt.

Later kan dezelfde informatie via een UI worden aangeboden.

Belangrijk:

```text
de UI is dus geen onderdeel van de machinecommunicatielaag.
```

De UI levert uiteindelijk alleen een keuze aan de CNCjsInterface.

---

## 23. Geen keuze nodig bij één machine

Wanneer er geen opgeslagen keuze bestaat maar CNCjs precies één geschikte machine aanbiedt, kan deze automatisch worden geopend.

Bijvoorbeeld:

```text
beschikbare machines:

1. Grbl /dev/ttyUSB0

→ automatisch openen
```

Wanneer er meerdere mogelijkheden zijn:

```text
1. Grbl /dev/ttyUSB0

2. TinyG /dev/ttyUSB1

→ keuze noodzakelijk
```

Dit voorkomt dat de pendant zonder expliciete keuze de verkeerde machine kan openen.

---

## 24. Machine openen

Wanneer een `AvailableMachine` is geselecteerd, wordt de verbinding via CNCjs geopend.

Voor GRBL is bijvoorbeeld getest:

```text
[
    "open",
    "/dev/ttyUSB0",
    {
        "controllerType": "Grbl",
        "baudrate": 115200,
        "rtscts": false,
        "pin": {
            "dtr": null,
            "rts": null
        }
    }
]
```

Na succesvol openen ontvangt de client bijvoorbeeld:

```text
["serialport:open", {
    "port": "/dev/ttyUSB0",
    "baudrate": 115200,
    "controllerType": "Grbl",
    "inuse": true
}]
```

Daarna kan CNCjs controllerinformatie leveren.

---

## 25. Controllerinformatie

Na het openen van de controller ontvangt de CNCjsInterface onder andere:

```text
controller:settings
```

en:

```text
controller:state
```

Bij GRBL bevat `controller:settings` bijvoorbeeld:

```text
version = 1.1h

$100 = 250.000
$101 = 250.000
$102 = 250.000

$110 = 500.000
$111 = 500.000
$112 = 500.000
```

Deze informatie kan later gebruikt worden om de capabilities van de machine te bepalen.

De CNCjsInterface hoeft deze gegevens echter niet zelf te interpreteren voor de JogPlanner.

De informatie wordt beschikbaar gesteld via een passend machine-model.

---

## 26. MachineCommand

De CNCjsInterface ontvangt vanuit de `MachineMapper` een `MachineCommand`.

Bijvoorbeeld:

```text
MachineCommand

    type     = GCODE

    port     = /dev/ttyUSB0

    command  = G0 X1
```

De CNCjsInterface vertaalt dit naar het CNCjs Socket.IO event:

```text
[
    "command",
    "/dev/ttyUSB0",
    "gcode",
    "G0 X1"
]
```

CNCjs stuurt dit vervolgens naar de geselecteerde controller.

De CNCjsInterface bepaalt dus niet wat een jog betekent.

Hij transporteert een reeds vertaald machinecommando.

---

## 27. Machinefeedback

CNCjs stuurt machinefeedback via Socket.IO events.

Een belangrijk event is:

```text
controller:state
```

Bijvoorbeeld:

```text
["controller:state", "Grbl", {
    "status": {
        "activeState": "Run",
        "mpos": {
            "x": "2.356",
            "y": "0.000",
            "z": "0.000"
        },
        "wpos": {
            "x": "2.356",
            "y": "0.000",
            "z": "0.000"
        },
        "feedrate": 186
    }
}]
```

De CNCjsInterface vertaalt deze informatie naar `MachineState`.

De JogPlanner hoeft dus geen CNCjs JSON te kennen.

---

## 28. MachineState is de werkelijkheid

De pendant gebruikt nooit zijn eigen verstuurde commando's als bewijs van de actuele machinepositie.

De werkelijkheid komt uit:

```text
controller:state
```

Dus:

```text
MachineCommand

    ≠

gerealiseerde beweging
```

De gerealiseerde beweging wordt vastgesteld via:

```text
CNCjs

   ↓

controller:state

   ↓

MachineState
```

Dit is essentieel voor de JogPlanner.

---

## 29. Externe bewegingen

De CNCjsInterface ontvangt ook bewegingen die niet door de pendant zijn gestart.

Bijvoorbeeld wanneer vanuit de CNCjs-webinterface een jog wordt gegeven.

De pendant ontvangt dan bijvoorbeeld:

```text
X = 2.004

X = 2.356

X = 2.932

X = 3.000
```

via opeenvolgende `controller:state` events.

Daarom blijft `MachineState` synchroon met de werkelijke machine, ongeacht waar het commando vandaan kwam.

Dit is expliciet getest.

---

## 30. Commando-bevestiging

CNCjs rapporteert ook communicatie met de controller.

Bijvoorbeeld:

```text
["serialport:write", "G0 X1\n", {...}]
```

gevolgd door:

```text
["serialport:read", "ok"]
```

Een `ok` betekent dat de controller het commando heeft geaccepteerd.

Dit is nuttige informatie voor communicatie- en foutdiagnostiek.

Het betekent echter niet noodzakelijk dat de beweging al voltooid is.

Daarvoor gebruiken we:

```text
controller:state
```

---

## 31. Machinebeschikbaarheid

De CNCjsInterface kent verschillende toestanden.

Conceptueel:

```text
DISCONNECTED

    ↓

AUTHENTICATING

    ↓

CONNECTED

    ↓

DISCOVERING

    ↓

MACHINE_SELECTION_REQUIRED

    ↓

OPENING

    ↓

READY
```

Bij verlies van de Socket.IO-verbinding gaat de client terug naar:

```text
DISCONNECTED
```

De rest van de applicatie kan hiermee bepalen of machinecommando's veilig verstuurd kunnen worden.

---

## 32. CNCjsInterface als zelfstandige taak

De `CNCjsInterface` is een zelfstandige FreeRTOS-taak.

De Socket.IO-communicatie wordt daardoor onafhankelijk verwerkt van de encoder-, button- en applicatietaken.

Conceptueel:

```text
CNCjsInterface Task

    │

    ├── Socket.IO communicatie

    ├── ontvangen events

    ├── verbindingstoestand

    ├── reconnects

    └── verwerking CNCjs feedback
```

De client mag de verwerking van fysieke input niet blokkeren.

Een tijdelijk trage netwerkverbinding mag bijvoorbeeld niet voorkomen dat de encoder verder wordt uitgelezen.

De communicatie tussen de CNCjsInterface en de overige applicatielagen verloopt daarom via daarvoor bestemde modellen en queues, niet via directe afhankelijkheid van de Socket.IO-loop.

De eerdere gedachte:

```text
CNCjsInterface.loop();
```

in de hoofdloop wordt daarmee vervangen door een zelfstandige communicatietaak.

De applicatielaag hoeft de Socket.IO-client dus niet voortdurend zelf aan te roepen.

---

## 33. Verantwoordelijkheden

De uiteindelijke verantwoordelijkheidsverdeling is:

```text
Encoder

    leest en interpreteert fysieke encoder

ButtonMatrix

    leest fysieke knopmatrix

InputManager

    normaliseert fysieke input

    debounce van matrixinput

    encoder- en knop-events samenvoegen

    encoderpulsen coalescen

    produceert Event-objecten

JogPlanner

    bepaalt WAT de gebruiker wil

MachineMapper

    bepaalt HOE die intentie wordt vertaald naar een
    machinecommando

CNCjsInterface

    bepaalt met WELKE CNCjs-machine wordt gecommuniceerd
    en verzorgt de communicatie

CNCjs

    beheert de seriële verbinding met de controller

GRBL / TinyG

    voert de daadwerkelijke motion control uit
```

---

## 34. Architectuur

De volledige input- en machinearchitectuur wordt:

```text
                  Encoder Task
                       │
                       │ EncoderEvent
                       ▼
                 InputManager
                       ▲
                       │
                       │ ButtonEvent
                       │
                ButtonMatrix Task
                       │
                       │
                       ▼
                 Input Queue
                       │
                       ▼
                  Application
                       │
                       ▼
                  JogPlanner
                       │
                       ├── gebruikershorizon
                       ├── resterende afstand
                       ├── gewenste aankomsttijd
                       └── gewenste feedrate
                       │
                       ▼
                 MachineMapper
                       │
                       ▼
                 MachineCommand
                       │
                       ▼
                CNCjs Command Queue
                       │
                       ▼
                 CNCjsInterface Task
                       │
                       ▼
                    CNCjs
                       │
                       ▼
                 GRBL / TinyG
                       │
                       ▼
                    machine
```

Machinefeedback loopt terug:

```text
machine

   ↓

GRBL / TinyG

   ↓

CNCjs

   ↓

CNCjsInterface Task

   ↓

MachineState

   ↓

JogPlanner / PendantController
```

---

## 35. Belangrijk ontwerpprincipe

De CNCjsInterface kent de technische wereld van CNCjs.

De JogPlanner kent de wereld van gebruikersintentie.

De MachineMapper vormt de brug tussen beide.

De InputManager vormt de brug tussen fysieke gebruikersinput en gebruikersintentie.

Daarmee geldt:

```text
Encoder

    "ik heb vier stappen naar rechts gedetecteerd"

        ↓

InputManager

    "de gebruiker wil vier stappen naar rechts"

        ↓

JogPlanner

    "de gebruikershorizon moet vier stappen opschuiven"

        ↓

MachineMapper

    "dit wordt vertaald naar een passend machinecommando"

        ↓

CNCjsInterface

    "ik stuur dit naar de geselecteerde CNCjs-machine"

        ↓

CNCjs

    "ik stuur het naar de seriële controller"

        ↓

GRBL / TinyG

    "ik voer de beweging uit"
```

Geen enkele laag hoeft de verantwoordelijkheden van de volgende laag over te nemen.