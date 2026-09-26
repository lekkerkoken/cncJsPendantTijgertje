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


TinyG native jog mapping toevoegen en testen
Doel

De bestaande JogCommand moet ook voor een TinyG-controller worden gemapt naar het native TinyG JSON jog-command.

Voor TinyG gebruiken we daarbij bewust de platte JSON-vorm:

{"jogx":-1.2}

Deze vorm sluit aan bij de bestaande TinyG JSON-commando's die in de huidige verbinding al succesvol worden gebruikt.

De mapping moet via de bestaande architectuur lopen:

JogCommand
    ↓
CNCjsInterface::execute()
    ↓
MachineMapper::map()
    ↓
MachineMapper::mapJogMove()
    ↓
MachineMapper::mapTinyGJog()
    ↓
{"jogx":-1.2}
    ↓
CNCjsInterface::sendGcode()
Huidige situatie

MachineMapper::mapJogMove() ondersteunt momenteel alleen GRBL:

case CONTROLLER_GRBL:

    return mapGrblJog(
        jog
    );


case CONTROLLER_TINYG:

    // TinyG jog mapping

    return unsupported();

Daardoor kan een JogCommand voor TinyG momenteel niet worden uitgevoerd.

De GRBL-mapping gebruikt:

$J=G91 X... F...

Voor TinyG willen we geen G-code-jog construeren.

TinyG heeft hiervoor een native JSON-command per as:

{"jogx":-1.2}

met overeenkomstige commands:

{"jogx":1}
{"jogy":1}
{"jogz":-1}
{"joga":1}

De waarde is de relatieve jog-afstand.

Gewenste implementatie

Voeg in MachineMapper een TinyG-specifieke mapping toe, bijvoorbeeld:

String mapTinyGJog(
    const JogCommand& jog
);

De mapping moet minimaal de volgende assen ondersteunen:

AXIS_X → jogx
AXIS_Y → jogy
AXIS_Z → jogz
AXIS_A → joga

De waarde van jog.delta wordt rechtstreeks als relatieve TinyG-jogafstand gebruikt.

Voorbeeld:

JogCommand
    axis  = AXIS_X
    delta = -1.2

moet worden:

{"jogx":-1.2}

Een mogelijke implementatie is bijvoorbeeld:

String MachineMapper::mapTinyGJog(
    const JogCommand& jog
)
{
    if(
        jog.axis == AXIS_NONE
    )
    {
        return unsupported();
    }

    const char* command = nullptr;

    switch(jog.axis)
    {
        case AXIS_X:
            command = "jogx";
            break;

        case AXIS_Y:
            command = "jogy";
            break;

        case AXIS_Z:
            command = "jogz";
            break;

        case AXIS_A:
            command = "joga";
            break;

        case AXIS_NONE:
        default:
            return unsupported();
    }

    char buffer[64];

    snprintf(
        buffer,
        sizeof(buffer),
        "{\"%s\":%.3f}",
        command,
        jog.delta
    );

    return String(buffer);
}

mapJogMove() wordt vervolgens:

case CONTROLLER_TINYG:

    return mapTinyGJog(
        jog
    );
Belangrijk: geen feedrate toevoegen

De TinyG native jog-command moet uitsluitend de relatieve afstand bevatten.

Dus niet:

{"jogx":-1.2,"feedrate":1000}

en ook niet:

{"jogx":-1.2,"f":1000}

maar:

{"jogx":-1.2}

De JogPlanner mag zijn feedrate dus voorlopig buiten deze TinyG mapping houden.

Test

De mapping moet niet alleen worden getest door handmatig een JSON-command naar TinyG te sturen.

De test moet de volledige bestaande route gebruiken:

JogCommand
    ↓
CNCjsInterface::execute()
    ↓
MachineMapper
    ↓
TinyG JSON
    ↓
CNCjs
    ↓
TinyG
Testcase 1 — X negatief

Maak een JogCommand met:

type  = JOG_MOVE
axis  = AXIS_X
delta = -1.2

Voer deze uit via:

cncjs.execute(jog);

De MachineMapper moet exact produceren:

{"jogx":-1.2}

De daadwerkelijke TinyG moet vervolgens de X-as 1,2 mm in negatieve richting bewegen.

De test is geslaagd wanneer zowel:

de mapping het verwachte JSON-command oplevert;
de TinyG daadwerkelijk de overeenkomstige beweging uitvoert.
Testcase 2 — X positief
axis  = AXIS_X
delta = 1.0

Verwacht:

{"jogx":1.000}

De TinyG moet X +1 mm bewegen.

Testcase 3 — Y
axis  = AXIS_Y
delta = -0.5

Verwacht:

{"jogy":-0.500}
Testcase 4 — Z
axis  = AXIS_Z
delta = 0.25

Verwacht:

{"jogz":0.250}
Testcase 5 — ongeldige as

Een JogCommand met:

axis = AXIS_NONE

moet geen TinyG-command opleveren.

MachineMapper::unsupported() moet worden gebruikt.

Testgrens

Deze eerste test richt zich uitsluitend op de mapping en uitvoering van één native TinyG jog-command.

Nog niet onderdeel van dit issue:

jog-cancel tijdens een lopende TinyG jog;
meerdere jog-commands achter elkaar;
jog buffering;
feedrate-configuratie;
xfr / yfr / zfr;
TinyG jog-state tijdens de beweging;
wijzigingen aan JogPlanner;
timing van de 50 ms slots.

Die zaken kunnen daarna afzonderlijk worden onderzocht.

Acceptatiecriteria

MachineMapper ondersteunt CONTROLLER_TINYG voor JOG_MOVE.

X wordt gemapt naar {"jogx":...}.

Y wordt gemapt naar {"jogy":...}.

Z wordt gemapt naar {"jogz":...}.

A wordt gemapt naar {"joga":...} indien AXIS_A in JogCommand beschikbaar is.

De waarde van jog.delta wordt als relatieve TinyG-jogafstand gebruikt.

Er wordt geen feedrate aan het TinyG JSON-command toegevoegd.

Een AXIS_NONE resulteert in unsupported().

De test wordt uitgevoerd via CNCjsInterface::execute() en niet door het JSON-command los van de pendant-architectuur te versturen.

Een test met delta = -1.2 produceert exact de TinyG-vorm:

{"jogx":-1.2}

De fysieke TinyG beweegt vervolgens 1,2 mm in negatieve X-richting.

De bestaande GRBL-mapping blijft ongewijzigd.


Maar eerst:

# Controller-specifieke serial-read verwerking en TinyG max-feedrates

## Doel

Controller-specifieke informatie die via `serialport:read` binnenkomt, moet beschikbaar kunnen worden gemaakt als controller-onafhankelijke domeininformatie voor de pendant.

Voor TinyG betekent dit onder andere dat de maximale feedrates:

```text
xfr
yfr
zfr
```

beschikbaar worden via:

```cpp
MachineSettings.maxFeedrate
```

De oplossing moet daarbij de scheiding tussen CNCjs-communicatie, controllerkennis en pendantlogica behouden.

---

# Architectuur

De verantwoordelijkheid wordt als volgt verdeeld:

```text
CNCjsClientCore
      │
      │ serialport:read
      ▼
CNCjsInterface
      │
      │ generieke controllerdata
      ▼
MachineMapper
      │
      │ controller-specifieke interpretatie
      ▼
┌──────────────────────────────────────┐
│ MachineSettings                      │
│ MachineState                         │
│ ControllerIdentification             │
│ MachineAlarm                         │
│ ...                                  │
└──────────────────────────────────────┘
      │
      ▼
PendantController
```

Het belangrijke uitgangspunt is:

> `CNCjsClientCore` hoeft niet te weten wat de ontvangen controllerdata betekent.

En:

> `CNCjsInterface` hoeft niet te weten dat `xfr` bij TinyG een maximale feedrate is.

Die kennis hoort bij `MachineMapper`.

---

# CNCjsClientCore

`CNCjsClientCore` blijft volledig controller-agnostisch.

Core is verantwoordelijk voor:

* Socket.IO-communicatie
* controller openen/sluiten
* CNCjs-events ontvangen
* `serialport:read` ontvangen
* de ontvangen `JsonArray` via `serialportReadHandler_` doorgeven
* generieke writes
* bestaande controller state/settings snapshots
* bestaande controllerselectie en verificatie

De bestaande callback blijft:

```cpp
std::function<void(const JsonArray&)> serialportReadHandler_;
```

De verwerking van `serialport:read` blijft in principe:

```cpp
if (serialportReadHandler_)
{
    serialportReadHandler_(array);
}
```

Er wordt geen TinyG- of GRBL-kennis aan `CNCjsClientCore` toegevoegd.

---

# CNCjsInterface

`CNCjsInterface` vormt de verbinding tussen `CNCjsClientCore` en de rest van de pendant.

De interface ontvangt generieke `serialport:read`-data van Core en geeft deze door aan de laag die de controller-taal kent.

De interface hoeft daarbij niet zelf te bepalen of bijvoorbeeld:

```json
{"r":{"xfr":16000},"f":[3,0,5]}
```

een settings-, state- of ander controllerbericht is.

Dat is controllerkennis en hoort niet in `CNCjsInterface`.

De interface kan dus conceptueel iets doen als:

```cpp
handleSerialRead(
    const JsonArray& data
);
```

en de controllerdata doorgeven aan `MachineMapper`.

---

# MachineMapper

`MachineMapper` is de laag die de controller-taal kent.

De mapper vertaalt controller-specifieke berichten naar controller-onafhankelijke pendant-domeintypes.

Een serial-read kan verschillende soorten informatie bevatten:

```text
serialport:read
       │
       ▼
MachineMapper
       │
       ├── MachineSettings
       ├── MachineState
       ├── ControllerIdentification
       ├── MachineAlarm
       └── ...
```

De mapper kan hiervoor bijvoorbeeld een `std::variant` retourneren:

```cpp
using MachineData = std::variant<
    MachineSettings,
    MachineState,
    ControllerIdentification,
    MachineAlarm
>;
```

Conceptueel:

```cpp
MachineData MachineMapper::map(
    const JsonDocument& data
) const;
```

of, afhankelijk van de bestaande `serialport:read` datastructuur, een passende variant daarvan.

De mapper heeft geen eigen permanente toestand nodig.

Hij vertaalt uitsluitend:

```text
controllerdata → pendantdata
```

---

# Controllerkeuze

De controllerkeuze blijft onderdeel van de bestaande verbindings- en verificatieflow.

De nieuwe serial-read verwerking mag de bestaande controllerselectie, verificatie of correctie niet wijzigen.

Nadat het daadwerkelijke controllertype bekend is, moet `MachineMapper` over voldoende informatie beschikken om de ontvangen data volgens de juiste controller-taal te interpreteren.

Bijvoorbeeld:

```text
MachineMapper
    │
    ├── TinyG → TinyG-interpretatie
    │
    └── GRBL  → GRBL-interpretatie
```

Dit betekent niet dat iedere serial-read verwerking een grote centrale `switch` hoeft te bevatten.

De controller-specifieke kennis mag intern worden georganiseerd op een manier die verdere uitbreiding naar andere controllers eenvoudig maakt.

---

# TinyG max-feedrates

TinyG publiceert `xfr`, `yfr` en `zfr` niet in de eerste `controller:settings`.

Wanneer het daadwerkelijke controllertype TinyG is vastgesteld, moeten deze waarden daarom expliciet worden opgevraagd.

De volgende TinyG-commando's worden verstuurd:

```json
{"xfr":null}
{"yfr":null}
{"zfr":null}
```

Elke JSON-regel wordt afgesloten met:

```text
\r
```

De bestaande generieke write-functionaliteit van `CNCjsClientCore` wordt hiervoor gebruikt.

Er hoeft hiervoor geen TinyG-specifieke communicatiefunctionaliteit aan Core te worden toegevoegd.

---

# TinyG response

Een TinyG-response kan bijvoorbeeld zijn:

```json
{"r":{"xfr":16000},"f":[3,0,5]}
```

De TinyG-specifieke interpretatie herkent hierin:

```text
r.xfr
```

als de maximale X-feedrate.

Op dezelfde manier worden:

```text
r.yfr
r.zfr
```

herkend.

De overige informatie in de response:

```text
f
```

hoeft voor deze functionaliteit niet te worden gebruikt.

---

# Partial responses

Een response hoeft niet alle drie de feedrates tegelijk te bevatten.

Bijvoorbeeld:

```json
{"r":{"xfr":15000},"f":[3,0,5]}
```

Als de huidige `MachineSettings` bevat:

```text
X = 16000
Y = 16000
Z = 12000
```

dan mag verwerking van bovenstaande response alleen X wijzigen:

```text
X = 15000
Y = 16000
Z = 12000
```

Ontbrekende waarden worden dus niet overschreven.

Hetzelfde geldt voor afzonderlijke `yfr`- en `zfr`-responses.

---

# MachineSettings

De controller-specifieke TinyG-waarden worden uiteindelijk vertaald naar:

```cpp
MachineSettings.maxFeedrate
```

Daarmee ziet de rest van de applicatie uitsluitend:

```cpp
settings.maxFeedrate.x
settings.maxFeedrate.y
settings.maxFeedrate.z
```

en hoeft deze code niets te weten van:

```text
xfr
yfr
zfr
```

of van TinyG.

---

# Belangrijk architectuurprincipe

De verantwoordelijkheid is daarmee:

```text
CNCjsClientCore
    ↓
"Ik heb controllerdata ontvangen."

CNCjsInterface
    ↓
"Ik geef die controllerdata door."

MachineMapper
    ↓
"Ik spreek de taal van de controller en bepaal wat deze data betekent."

PendantController
    ↓
"Ik werk met MachineSettings, MachineState, MachineAlarm, enz."
```

`MachineMapper` bewaart geen machinegegevens en is geen eigenaar van de actuele `MachineSettings` of `MachineState`.

Het is uitsluitend een vertaallaag.

---

# Acceptatiecriteria

* `CNCjsClientCore` bevat geen TinyG-kennis.
* `CNCjsClientCore` blijft `serialport:read` generiek doorgeven.
* `CNCjsInterface` hoeft niet te weten wat `xfr`, `yfr` of `zfr` betekenen.
* Controller-specifieke interpretatie vindt plaats in `MachineMapper`.
* TinyG `xfr`, `yfr` en `zfr` kunnen expliciet worden opgevraagd nadat TinyG als daadwerkelijke controller is vastgesteld.
* Een TinyG-response met slechts één feedrate wijzigt alleen die feedrate.
* `MachineSettings.maxFeedrate` bevat uiteindelijk de controller-onafhankelijke waarden.
* Bestaande controllerselectie en sessieverificatie blijven ongewijzigd.
* De oplossing maakt het mogelijk om later andere controller-specifieke serial-read informatie op dezelfde manier te vertalen.


COMMITS COMMITS COMMITS
1. Pass serialport:read data from Core to Interface

Doel: een generieke route creëren voor controllerdata die via serialport:read binnenkomt.

CNCjsClientCore
      │
      │ serialport:read
      ▼
CNCjsInterface

In deze commit:

CNCjsClientCore blijft volledig controller-agnostisch.
serialportReadHandler_ blijft de bestaande callback.
CNCjsInterface krijgt de ontvangen JsonArray.
Nog geen interpretatie van de inhoud.
Nog geen TinyG-code.
Bestaande controllerselectie/verificatie blijft ongemoeid.

Commit:

Pass serialport:read data from Core to Interface
2. Publish mapped machine data from Interface

Dit is de architecturale stap die we zojuist scherp hebben gekregen.

De huidige gedachte:

PendantController
      ↓
machineSettingsSnapshot()
      ↓
"haal alle relevante settings ergens vandaan"

werkt niet controller-onafhankelijk.

We willen naar:

CNCjs data
     ↓
CNCjsInterface
     ↓
MachineMapper
     ↓
machine data
     ↓
PendantController

In deze commit leggen we dus vast dat binnenkomende data actief verwerkt en gepubliceerd kan worden.

Belangrijk:

MachineMapper blijft stateless.
CNCjsInterface blijft de adapter tussen CNCjs en de pendant.
PendantController krijgt domeininformatie, geen CNCjs-events.
De bestaande machineSettingsSnapshot()-benadering wordt niet verder uitgebouwd.
Als er bestaande snapshot-code nodig is voor de overgang, laten we die tijdelijk bestaan waar nodig.

Nog steeds geen TinyG-specifieke xfr-logica.

Commit:

Publish mapped machine data from Interface
3. Map controller-specific serial data in MachineMapper

Nu krijgt MachineMapper daadwerkelijk een taak voor serialport:read.

De hoofdlijn wordt:

serialport:read
       │
       ▼
CNCjsInterface
       │
       ▼
MachineMapper
       │
       ├── MachineSettings
       ├── MachineState
       ├── ControllerIdentification
       ├── MachineAlarm
       └── ...

Hier bepalen we ook hoe verschillende soorten controllerdata worden onderscheiden.

Bijvoorbeeld conceptueel:

MachineData MachineMapper::mapSerialData(...)

waarbij MachineData eventueel een std::variant kan zijn.

Maar dat is een implementatiedetail van deze commit. Het hoofddoel is:

Controller-specifieke betekenis wordt uitsluitend in MachineMapper bepaald.

Nog steeds hoeft deze commit geen TinyG max-feedrate te implementeren. We leggen eerst het generieke mechanisme vast.

Commit:

Map controller-specific serial data in MachineMapper
4. Map TinyG maximum feedrates from serial responses

Nu komt de concrete TinyG-functionaliteit.

Een response als:

{"r":{"xfr":16000},"f":[3,0,5]}

wordt door MachineMapper geïnterpreteerd als:

MachineSettings.maxFeedrate.x = 16000

en:

r.yfr → maxFeedrate.y
r.zfr → maxFeedrate.z

Hier hoort ook meteen het partial-response gedrag bij.

Dus:

bestaand:
X = 16000
Y = 16000
Z = 12000

ontvangen:
xfr = 15000

resultaat:
X = 15000
Y = 16000
Z = 12000

Ontbrekende waarden worden niet overschreven.

Daarmee testen we eigenlijk de belangrijkste inhoudelijke vertaling van dit issue:

TinyG-taal
    ↓
MachineMapper
    ↓
MachineSettings

Commit:

Map TinyG maximum feedrates from serial responses
5. Request TinyG maximum feedrates after controller verification

Pas nu zorgen we ervoor dat de waarden ook daadwerkelijk beschikbaar komen.

Nadat de controller als TinyG is vastgesteld:

TinyG verified
      │
      ├── {"xfr":null}\r
      ├── {"yfr":null}\r
      └── {"zfr":null}\r

via de bestaande generieke write-functionaliteit.

De flow wordt dan compleet:

                 controller verification
                         │
                         ▼
                       TinyG
                         │
                request xfr/yfr/zfr
                         │
                         ▼
                  serialport:read
                         │
                         ▼
                  CNCjsInterface
                         │
                         ▼
                   MachineMapper
                         │
                         ▼
                  MachineSettings
                         │
                         ▼
                 PendantController

Hiermee blijft:

Core → communicatie;
Interface → adapter/coördinator;
Mapper → controllerkennis;
PendantController → domeinlogica.

Commit:

Request TinyG maximum feedrates after controller verification
Uiteindelijk dus vijf commits
1. Pass serialport:read data from Core to Interface

2. Publish mapped machine data from Interface

3. Map controller-specific serial data in MachineMapper

4. Map TinyG maximum feedrates from serial responses

5. Request TinyG maximum feedrates after controller verification
Waarom ik het hierbij zou laten

Ik zou geen aparte cleanup-commit meer plannen. Cleanup hoort gewoon onderdeel te zijn van de relevante commit.

En ik zou ook geen aparte commit maken voor MachineSettings. De belangrijke architectuurverandering is niet "MachineSettings uitbreiden", maar dat MachineSettings voortaan een resultaat van controllerdata kan zijn, in plaats van dat CNCjsInterface::machineSettingsSnapshot() probeert alle mogelijke controllerarchitecturen achteraf bij elkaar te schrapen.

Dat onderscheid is volgens mij nu de hoofdlijn van het hele issue.