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
* reageren op de toestand van `CNCjsClient`;
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
     └──────────────► CNCjsClient
```

De afzonderlijke verantwoordelijkheden blijven gescheiden:

| Component                | Verantwoordelijkheid                                     |
| ------------------------ | -------------------------------------------------------- |
| `InputManager`           | Fysieke inputs vertalen naar `Event`                     |
| `PendantController`      | Pendantgedrag en displayprioriteit                       |
| `PendantState`           | Toestand van de bediening                                |
| `Display`                | Weergeven van informatie                                 |
| `MachineState`           | Toestand van de CNC-machine                              |
| `CNCjsClient`            | WiFi, mDNS, authenticatie, Socket.IO en CNCjs-controller |
| `JogPlanner`             | Plannen van jogbewegingen                                |
| `MachineMapper`          | Vertalen tussen machine- en pendantcoördinaten           |
| `PowerManager` *(later)* | Sleep-, wake- en energiebeheer                           |

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
LAYER_CONTROL
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

De `CNCjsClient` beheert de volledige technische verbinding met CNCjs.

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

Bij verbindingsverlies kan `CNCjsClient` zelfstandig opnieuw proberen verbinding te maken. Zodra de status verandert, kan `PendantController` de displayweergave daarop aanpassen.

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

Daardoor kunnen WiFi, CNCjs, machinecommunicatie, jogplanning en energiebeheer onafhankelijk worden ontwikkeld zonder dat de pendantlogica daarmee verweven raakt.
