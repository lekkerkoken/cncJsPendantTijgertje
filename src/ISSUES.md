# ISSUE — PendantLayers: eventhandlers per layer registreren

## Doel

De pendant moet met afzonderlijke `PendantLayer`s gaan werken waarbij iedere layer bij activatie bepaalt **welke functies aan de bestaande eventhandlers worden gekoppeld**.

De `PendantController` blijft de centrale orchestrator, maar hoeft niet zelf te bepalen wat een specifiek event binnen iedere layer betekent.

## Kernidee

De pendant heeft vaste eventhandlers voor de verschillende input-events. Deze handlers hebben standaardfuncties, zodat ieder event altijd veilig afgehandeld kan worden.

Conceptueel:

```cpp
key8EventHandler = &defaultKeyHandler;
key4EventHandler = &defaultKeyHandler;
key7EventHandler = &defaultKeyHandler;

encoderEventHandler      = &defaultEncoderHandler;
encoderPressEventHandler = &defaultEncoderPressHandler;
```

Wanneer een layer wordt geactiveerd, registreert die layer de functies die bij haar bediening horen:

```cpp
// JogLayer activeren

key8EventHandler = &activateXJogFunction;
key4EventHandler = &activateYJogFunction;
key7EventHandler = &activateZJogFunction;

encoderEventHandler      = &jogEncoderFunction;
encoderPressEventHandler = &toggleLayerFunction;
```

Een andere layer kan vervolgens dezelfde eventhandlers anders configureren:

```cpp
// InfoLayer activeren

key8EventHandler = &infoFunction;
key4EventHandler = &infoFunction;
key7EventHandler = &infoFunction;

encoderEventHandler      = &infoEncoderFunction;
encoderPressEventHandler = &toggleLayerFunction;
```

De layer **handelt dus niet zelf een `Event` af**. De layer configureert de functies die door de bestaande eventhandlers worden aangeroepen.

## Layerwisseling

Een layer switch betekent conceptueel:

```text
huidige layer verlaten
        ↓
eventhandlers resetten naar defaults
        ↓
nieuwe layer actief maken
        ↓
functies van nieuwe layer registreren bij eventhandlers
        ↓
nieuwe layer enter/lifecycle uitvoeren
```

Daarmee is de input-routing na een layer switch volledig opnieuw geconfigureerd.

## PendantController

De `PendantController` blijft verantwoordelijk voor:

* lifecycle van het pendant-systeem;
* actieve layer;
* ontvangen events vanuit `InputManager`;
* uitvoeren van de geregistreerde eventhandlers;
* layer switching;
* display- en machine-statuscoördinatie;
* communicatie tussen de verschillende subsystemen.

De controller hoeft **niet** langer een grote `switch(layer)` te bevatten om voor ieder event te bepalen wat het betekent.

## JogLayer

`JogLayer` wordt een echte layer en registreert bij activatie onder andere:

```text
KEY_8        → X jog activeren
KEY_4        → Y jog activeren
KEY_7        → Z jog activeren
KEY_6        → jogstep kleiner
KEY_9        → jogstep groter
ENCODER      → JogPlanner encoder input
ENCODER PRESS → layer switch
```

De bestaande `JogPlanner` blijft verantwoordelijk voor de joglogica. De layer bepaalt alleen hoe de pendant-input aan die functionaliteit wordt gekoppeld.

## ALARM is geen normale PendantLayer

`ALARM` wordt niet gemodelleerd als een gewone layer zoals `JOG`, `INFO` of `CONTROL`.

Een alarm is een **machineconditie met hogere prioriteit** die over de normale layer heen ligt.

Conceptueel:

```text
                 PendantController
                        │
              ┌─────────┴─────────┐
              │                   │
       system/status         layer handlers
         handlers          JOG / INFO / CONTROL
              │
            ALARM
```

De actieve layer blijft bijvoorbeeld `JOG`, maar zolang de machine in `MACHINE_ALARM` staat kunnen alarm/system-handlers prioriteit krijgen boven de normaal geregistreerde layer-handlers.

Dit principe moet verder worden uitgewerkt voordat de definitieve handlerarchitectuur wordt geïmplementeerd.

## Belangrijke ontwerpbeslissingen voor implementatie

Nog vast te leggen:

1. Hoe de eventhandlers technisch worden opgeslagen.

   * function pointers;
   * `std::function`;
   * eigen handlerstructuur;
   * of een andere lichte Arduino-geschikte oplossing.

2. Welke input-events een eigen handler krijgen.

3. Welke handlers standaard beschikbaar zijn en wat hun default gedrag is.

4. Hoe een layer zijn handlers registreert.

   * bijvoorbeeld `registerHandlers()`;
   * of via een expliciete `enter()`/`activate()` lifecycle.

5. Hoe system/status-handlers zoals `ALARM`, `DISCONNECTED` en eventueel `HOLD` zich verhouden tot de normale layer-handlers.

6. Of layer-specifieke state behouden blijft bij verlaten en opnieuw betreden van een layer, of opnieuw wordt geïnitialiseerd.

## Niet het doel

Deze architectuur is nadrukkelijk niet bedoeld om:

* alle functionaliteit in `PendantLayer`-klassen te stoppen;
* `JogPlanner`, `CNCjsInterface` of andere domeincomponenten onderdeel van een layer te maken;
* een generiek event-framework te bouwen;
* de `PendantController` volledig uit te hollen.

De `PendantController` blijft de centrale orchestrator. De layers bepalen vooral **welke functies aan de input-eventhandlers gekoppeld zijn wanneer die layer actief is**.

Issue 1 — Onderzoek controller:state

Doel: vaststellen welke informatie we daadwerkelijk uit controller:state kunnen halen, met name of daar iets in zit waarmee we homing/readiness kunnen bepalen.

Stappen:

Log tijdelijk iedere ontvangen controller:state.
Laat daarbij de volledige JSON zien, niet alleen de velden die MachineMapper nu gebruikt.
Voer verschillende situaties uit:
CNCjs verbonden, machine niet gehomed
machine gehomed
machine Idle
machine Run
machine Alarm
eventueel na opnieuw verbinden/opstarten
Vergelijk de JSON's.
Zoek specifiek naar:
homing/home-status
alarm/status
machine position
work position
WCS
eventuele controller-specifieke flags
Besluiten welke informatie daadwerkelijk betrouwbaar genoeg is om in MachineState op te nemen.

Resultaat: geen aannames over homed, maar een concreet overzicht van wat controller:state ons geeft.

Issue 2 — Command-display maken

Hier zou ik nu nog niet meteen de inhoud van het display vastleggen. Wel kunnen we de implementatiestappen al heel concreet maken.

Stap 1 — Command-layer als display-context

PendantController zorgt dat bij:

setLayer(LAYER_COMMAND)
        ↓
enterCommandLayer()

de Command-layer actief wordt en de display-context wordt ingesteld.

Het logo staat, net als bij Jog, links:

┌──────────────────────────────┐
│ [COMMAND_ICON]   ...         │
│                              │
│                              │
└──────────────────────────────┘

Dus dezelfde visuele grammatica als Jog.

Stap 2 — Display krijgt een specifieke Command-render

Niet alles in één algemene updateNormalDisplay() proppen.

Bijvoorbeeld conceptueel:

updateDisplay()
    │
    ├── JOG
    │     └── updateJogDisplay()
    │
    ├── INFO
    │     └── updateInfoDisplay()
    │
    └── COMMAND
          └── updateCommandDisplay()

De Display zelf blijft verantwoordelijk voor tekenen; PendantController bepaalt wat er getoond moet worden.

Stap 3 — Eerst de beschikbare machine-informatie gebruiken

De eerste versie van updateCommandDisplay() gebruikt alleen informatie die we al betrouwbaar hebben:

machineStatus
machinePosition
workPosition
activeWcs
feedrate
spindleSpeed

En eventueel later:

homed
job
progress
...

maar pas nadat Issue 1 heeft vastgesteld waar die informatie vandaan komt.

Stap 4 — Displayinhoud vanuit de workflow ontwerpen

De Command-layer moet niet voelen als:

"hier staan nog wat CNC-commando's"

maar als:

de machine staat klaar om werk te starten of gecontroleerd te beëindigen.

Daarom eerst de toestanden bepalen:

COMMAND
  │
  ├── klaar / READY
  │
  ├── bezig / RUN
  │
  ├── gepauzeerd / HOLD
  │
  ├── gestopt / IDLE
  │
  └── probleem / ALARM

Daarna bepalen we per toestand wat rechts van het logo relevant is.

Stap 5 — Knoppen koppelen aan die workflow

Voorlopig:

1  Feed Hold / Resume
2  Start / Pause
3  Stop + bevestiging
4  Home Y + bevestiging
5  Home All + bevestiging
6  ongebruikt
7  Home Z + bevestiging
8  Home X + bevestiging
9  ongebruikt

Daarbij blijven 6 en 9 bewust leeg. We hoeven die niet kunstmatig te vullen.

Stap 6 — Pas daarna eventueel MachineState uitbreiden

Als Issue 1 bijvoorbeeld aantoont dat controller:state een betrouwbare homing-indicatie bevat, kunnen we gericht toevoegen:

bool homed = false;

aan MachineState, en vervolgens:

controller:state
      ↓
MachineMapper
      ↓
MachineState.homed
      ↓
Command display

Dat lijkt me een veel betere volgorde dan nu alvast een homed-veld toevoegen.

Kortom: eerst Issue 1 uitvoeren. Daarna hebben we de echte gegevens waarop we het Command-display kunnen ontwerpen.


# MacroSnapshot en eenmalig ophalen van CNCjs-macro's

## Doel

De CNCjs-pendant moet bij het opstarten de beschikbare macro's uit CNCjs kunnen ophalen.

CNCjs biedt hiervoor:

```text
GET /api/macros
Authorization: Bearer <JWT>
```

De macro's hoeven voorlopig **slechts één keer per verbinding/opstart** opgehaald te worden. Er is daarom geen periodieke synchronisatie of aparte request-queue nodig.

## Scope

In deze stap bouwen we alleen de communicatie tussen `CNCjsClientCore` en CNCjs.

We voegen:

* een kleine `MacroSnapshot` toe aan `CNCjsClientCore`;
* een methode toe om de macro's via `GET /api/macros` op te halen;
* het ophalen uit zodra de CNCjs-verbinding daadwerkelijk `Ready` is;
* de opgehaalde JSON beschikbaar via een snapshot.

De bestaande `MacroManager` wordt in deze stap **nog niet gevuld**.

De vertaling van de CNCjs-response naar `MacroInfo` en het vullen van `MacroManager` volgt in een apart issue.

## Architectuur

De verantwoordelijkheden blijven gescheiden:

```text
PendantController
        │
        ▼
CNCjsInterface
        │
        ▼
CNCjsClientCore
        │
        ├── NetworkManager
        ├── Socket.IO
        └── HTTP GET /api/macros
                    │
                    ▼
                  CNCjs
```

`MacroManager` blijft onafhankelijk van CNCjs:

```text
CNCjsClientCore
      │
      │ MacroSnapshot
      ▼
CNCjsInterface
      │
      │ MacroInfo[]
      ▼
MacroManager
```

## Belangrijke ontwerpkeuzes

### 1. CNCjs blijft source of truth

De pendant bewaart de macro's lokaal alleen als runtime-state.

De lijst wordt door CNCjs geleverd en is dus niet zelfstandig editable vanuit de pendant.

### 2. Eénmalig ophalen

Het ophalen gebeurt nadat CNCjs:

* met WiFi verbonden is;
* de CNCjs-server heeft gevonden;
* succesvol geauthenticeerd is;
* en de Socket.IO-verbinding klaar is.

Daarna wordt `/api/macros` niet periodiek opnieuw aangeroepen.

### 3. JWT blijft intern

De JWT die tijdens authenticatie door `CNCjsClientCore` wordt verkregen, blijft onderdeel van de CNCjs-communicatielaag.

`MacroManager` en `PendantController` krijgen geen JWT en hoeven niets van authenticatie te weten.

### 4. Bestaande mutex gebruiken

De bestaande `CNCjsClientCore::lock()` / `unlock()`-mechaniek wordt gebruikt.

Er wordt geen tweede mutex geïntroduceerd.

HTTP-functionaliteit (`HTTPClient.h`, `WiFiClient.h`) blijft een implementation detail van `CNCjsClientCore.cpp` en hoeft niet in het publieke `.h`-bestand.

### 5. Geen nieuwe netwerk-task

Omdat het ophalen slechts één keer gebeurt, voegen we geen aparte HTTP-task of request-queue toe.

De bestaande netwerkarchitectuur blijft leidend.

## MacroSnapshot

Maak een eenvoudige snapshot die vergelijkbaar is met de bestaande state/settings snapshots.

De snapshot bevat minimaal:

```text
valid
macros
```

waarbij `macros` de door CNCjs geretourneerde JSON bevat.

De snapshot moet ongeldig kunnen worden gemaakt wanneer de macro-data niet langer geldig is.

## Acceptatiecriteria

* [ ] `MacroSnapshot` bestaat als aparte snapshot-structuur.
* [ ] `CNCjsClientCore` beschikt over een `macroSnapshot()` accessor.
* [ ] `CNCjsClientCore` kan `GET /api/macros` uitvoeren.
* [ ] De bestaande JWT wordt gebruikt als `Authorization: Bearer <JWT>`.
* [ ] De HTTP-functionaliteit blijft intern in `CNCjsClientCore.cpp`.
* [ ] De bestaande `lock()` / `unlock()` wordt gebruikt.
* [ ] Het ophalen gebeurt pas nadat de CNCjs-verbinding `Ready` is.
* [ ] Het ophalen gebeurt slechts één keer.
* [ ] Een succesvolle response wordt in `MacroSnapshot` opgeslagen.
* [ ] Bij een mislukte HTTP-call of ongeldige JSON wordt de snapshot niet als geldig beschouwd.
* [ ] `MacroManager` wordt in dit issue nog niet aangepast of gevuld.
* [ ] Geen wijzigingen aan jog-, layer- of displayfunctionaliteit.

## Niet in scope

Deze zaken volgen later:

* JSON → `MacroInfo` mapping;
* vullen van `MacroManager`;
* macro's selecteren op de pendant;
* macro's uitvoeren;
* `macro:run`;
* runtime `context` meegeven aan een macro;

