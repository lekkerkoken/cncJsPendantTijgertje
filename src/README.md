# cncJsPendantTijgertje

## CNCjs client architecture

The CNCjs client is deliberately split into three responsibilities:

```text
                        Application
                             │
                             ▼
                    ┌─────────────────┐
                    │ CNCjsInterface  │
                    │                 │
                    │ Application API │
                    │ State Snapshot  │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │ CNCjsClientCore │
                    │                 │
                    │ CNCjs protocol  │
                    │ Socket.IO       │
                    │ Controller      │
                    │ Machine state   │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │ NetworkManager  │
                    │                 │
                    │ WiFi            │
                    │ DNS             │
                    │ HTTP auth       │
                    └─────────────────┘
```

### NetworkManager

NetworkManager is responsible for network-level services only:

* WiFi connection
* DNS resolution
* HTTP communication
* HTTP authentication
* obtaining the CNCjs authentication token

NetworkManager does not know about CNCjs protocol messages, Socket.IO events, controllers or machine state.

### CNCjsClientCore

CNCjsClientCore is the implementation of the CNCjs client.

It is responsible for:

* the CNCjs connection state machine
* Socket.IO communication
* CNCjs protocol and event handling
* controller and serial-port discovery
* controller selection and opening
* the CNCjs heartbeat
* processing CNCjs machine-state information
* updating MachineState
* executing and transmitting machine commands and G-code
* maintaining the internal CNCjs state used by the interface

The network processing runs in the CNCjsClientCore FreeRTOS network task.

CNCjsClientCore uses NetworkManager for WiFi, DNS and HTTP authentication, but does not implement those services itself.

### CNCjsInterface

CNCjsInterface is the application-facing boundary of the CNCjs client.

The rest of the application communicates with CNCjs through this interface and should not depend directly on the implementation details of CNCjsClientCore.

The interface is responsible for:

* exposing the CNCjs functionality required by the application
* providing a stable application-facing API
* exposing connection, controller and machine status
* providing a consistent snapshot of the current CNCjs state
* forwarding application commands to CNCjsClientCore

CNCjsInterface does not implement:

* Socket.IO
* WiFi
* DNS
* HTTP authentication
* CNCjs protocol parsing
* the network task
* CNCjs connection handling

### State and synchronization

CNCjsClientCore owns and updates the live CNCjs state.

The application consumes that state through CNCjsInterface as a consistent snapshot rather than depending on individual internal Core fields.

The network task and application-facing access are synchronized using the CNCjs network mutex. The synchronization boundary must be preserved when the implementation is refactored.

### Architectural rule

The separation is intentional:

```text
NetworkManager

    provides network services.


CNCjsClientCore

    implements CNCjs.


CNCjsInterface

    provides the application API.
```

New functionality should be placed according to these responsibilities rather than simply added to the class that happens to be easiest to access.

# JogPlanner – tijdgebaseerd intentiemodel

## Doel

De JogPlanner vertaalt menselijke encoderinput naar een **tijdgebonden bewegingsintentie** voor de CNC-machine.

De encoder wordt niet gezien als een knop die een reeks onafhankelijke machinebewegingen produceert.

De gebruiker geeft voortdurend aan:

```text
"Ik wil dat de machine deze kant op beweegt,
met ongeveer deze intensiteit."
```

De planner vertaalt deze intentie naar een korte beweging in de nabije toekomst.

De belangrijkste eigenschap van het model is dat **intentie een beperkte geldigheid heeft**.

Een intentie die te oud is om nog als reactie op de huidige gebruikersinput te voelen, mag niet alsnog door de machine worden uitgevoerd.

## 1. Geen absolute horizon meer

Het oude model gebruikte een absolute gebruikershorizon:

```text
machinepositie → horizon
```

Concepten zoals:

```text
horizon
target
communicatedTarget
jogActive
```

zijn daarom geen fundamentele onderdelen meer van het nieuwe model.

De planner hoeft niet te onthouden:

```text
"de gebruiker wil uiteindelijk op positie X uitkomen."
```

In plaats daarvan onthoudt de planner:

```text
"dit is de bewegingsintentie voor de eerstvolgende
stukken van de tijd."
```

De machinepositie uit `MachineState` blijft de werkelijkheid.

De planner probeert daar niet een tweede voorspelde positie naast te zetten.

# 2. De intentiering

De gebruikersintentie wordt opgeslagen in een **ringbuffer**.

De ring is een tijdvenster rond het heden.

Voorlopig gebruiken we:

```text
INTENT_LIFETIME = 0,5 s
PLAN_AHEAD      = 0,5 s
PLANNER_RATE    = 20 Hz
```

Bij 20 Hz duurt één tijdslot:

```text
1 / 20 = 0,05 s
```

Daarmee ontstaan:

```text
0,5 / 0,05 = 10 slots
```

De ring bevat dus voorlopig **10 tijdslots**.

## 3. De ring is een tijdas

Conceptueel ziet de ring eruit als een bewegend planvenster:

```text
                     TOEKOMST
                        →
                        
        ┌─────┬─────┬─────┬─────┬─────┐
        │  1  │  2  │  3  │ ... │  10 │
        └─────┴─────┴─────┴─────┴─────┘
          ↑
         NU
```

Maar de ring zelf is circulair.

Terwijl de tijd verstrijkt, schuift het betekenisvolle tijdvenster door de ring:

```text
                    PLAN AHEAD
                ──────────────────►

        NET        NU              TOEKOMST
         │          │                  │
         ▼          ▼                  ▼

      [verlopen] [actueel] [1] [2] [3] ... [10]
```

Een slot dat achter `NU` terechtkomt is verlopen.

Een slot dat verder dan `PLAN_AHEAD` ligt, mag niet worden gevuld.

De ring is daarmee geen queue van commando's.

Het is een **tijdvenster van toekomstige bewegingsintentie**.

# 4. Wat bevat een slot?

Een slot bevat uitsluitend een bewegingsdelta:

```text
delta movement
```

Bijvoorbeeld:

```text
slot 0   +0,010 mm
slot 1   +0,010 mm
slot 2   +0,010 mm
slot 3   +0,010 mm
...
```

Het slot zegt daarmee:

```text
"Tijdens dit tijdsinterval wil de gebruiker
deze hoeveelheid beweging."
```

Het slot bevat dus geen zelfstandig toekomstig absoluut target.

De fysieke machinepositie wordt door GRBL bepaald.

# 5. Encoderinput vult de toekomst

Een encoderbeweging is een nieuwe gebruikersintentie.

Wanneer de gebruiker bijvoorbeeld één stap van:

```text
+0,1 mm
```

geeft, wordt deze intentie niet als één grote beweging in één slot geplaatst.

De stap wordt verdeeld over het beschikbare planvenster.

Bij tien slots betekent dit bijvoorbeeld:

```text
encoder:

+0,1 mm

        ↓

slot 1   +0,01
slot 2   +0,01
slot 3   +0,01
slot 4   +0,01
slot 5   +0,01
slot 6   +0,01
slot 7   +0,01
slot 8   +0,01
slot 9   +0,01
slot 10  +0,01
```

De precieze verdeling is onderdeel van het planneralgoritme, maar het principe is:

> Een nieuwe encoderintentie wordt uitgesmeerd over de nabije toekomst.

Daardoor ontstaat geen plotselinge grote machinebeweging als reactie op één encoderpuls.

# 6. De verhouding tussen NU en TOEKOMST

De planner houdt het tijdvenster bewust kort.

Voorlopig geldt:

```text
INTENT_LIFETIME = 0,5 s
PLAN_AHEAD      = 0,5 s
```

Dit betekent dat de planner ongeveer evenveel toekomst als directe tijd representeert.

Conceptueel:

```text
              NU
               │
               ▼
NET ───────────┼────────────── TOEKOMST
               │
        <------┼------------->
            0,5 seconde
```

De verhouding tussen actuele tijd en geplande toekomst moet dicht bij `1:1` blijven.

Een veel langere toekomst zou betekenen dat een encoderbeweging pas veel later zichtbaar wordt.

Dat voelt niet meer als directe bediening.

Een veel kortere toekomst zou juist weinig ruimte laten voor de communicatie- en motion-planningketen.

# 7. Intentie heeft een houdbaarheid

Dit is een fundamenteel onderdeel van het model.

Een encoderbeweging van bijvoorbeeld 500 ms geleden is niet automatisch nog steeds relevante gebruikersintentie.

Als die intentie nog niet door de machine is opgevolgd wanneer het betreffende tijdslot verloopt, wordt deze vergeten.

Dus:

```text
nieuwe intentie
      ↓
ringbuffer
      ↓
tijd verstrijkt
      ↓
slot bereikt NU
      ↓
slot wordt uitgevoerd
      ↓
slot verloopt
      ↓
verdwijnt uit de ring
```

Niet:

```text
oude intentie
      ↓
niet verstuurd
      ↓
later alsnog versturen
```

Dat laatste zou namelijk een fundamenteel verkeerd gebruikersgevoel veroorzaken:

```text
gebruiker draait
    ↓
wacht
    ↓
draait inmiddels terug
    ↓
oude beweging wordt alsnog uitgevoerd
```

De machine zou dan reageren op de geschiedenis in plaats van op de actuele intentie.

# 8. Verlopen intentie valt aan de achterkant uit de ring

De ring heeft daarmee een natuurlijke vorm van garbage collection.

Wanneer een slot verloopt:

```text
[ A ][ B ][ C ][ D ][ E ][ F ][ G ][ H ][ I ][ J ]
  ↑
  verlopen
```

wordt het slot leeggemaakt.

Daarna kan dezelfde fysieke positie in de ring opnieuw worden gebruikt voor de toekomst:

```text
[ nieuwe ][ B ][ C ][ D ][ E ][ F ][ G ][ H ][ I ][ J ]
```

De ring hoeft dus nooit groter te worden om steeds nieuwe intentie te bevatten.

De tijd maakt oude intentie vanzelf ongeldig.

# 9. Richtingsverandering

Een richtingsverandering is geen reden om eerst de oude beweging volledig uit te voeren.

De nieuwe intentie moet de bestaande toekomstige intentie onmiddellijk beïnvloeden.

Bijvoorbeeld:

```text
toekomstige intentie:

+ + + + + + + + + +
```

De gebruiker draait terug:

```text
-
```

De planner breekt de bestaande toekomstige intentie geleidelijk af.

Conceptueel:

```text
+ + + + + + + + + +

        ↓ terugdraaien

+ + + + + - - - - -
```

Daarbij wordt de bestaande delta herhaaldelijk gehalveerd:

```text
oude delta

    ↓ /2

    ↓ /2

    ↓ /2

    ...
```

totdat de resterende delta kleiner wordt dan een kleine plannergrens.

Daarna wordt de nieuwe tegengestelde stap over de toekomstige slots verdeeld.

Het belangrijke principe is:

> De nieuwe gebruikersintentie wordt niet achter de oude intentie geplaatst. De toekomstige intentie zelf wordt aangepast.

# 10. Geen cumulatieve straf voor intentie-afbraak

Het afbreken van oude intentie mag niet leiden tot een cumulatieve vertraging.

Als een gebruiker:

```text
+
+
+
+
-
-
-
```

draait, moet de machine niet uiteindelijk een lange reeks oude positieve bewegingen uitvoeren omdat die ooit in de planner terechtkwamen.

De oude positieve intentie valt vanzelf aan de achterkant uit de ring.

Daarmee is de hoeveelheid historische "schuld" begrensd door:

```text
INTENT_LIFETIME
```

en niet door hoeveel encoderinput er in het verleden is geweest.

# 11. Fysieke machinebeweging is het resultaat van alle slots

De slots zijn **geen fysieke machinebewegingen**.

GRBL ontvangt jogbewegingen en bepaalt vervolgens zelf hoe de machine fysiek accelereert, beweegt en afremt.

Conceptueel:

```text
Encoder
   ↓
Intentiering
   ↓
tijdslot
   ↓
$J
   ↓
GRBL motion planner
   ↓
fysieke beweging
```

De uiteindelijke machinebeweging is dus het resultaat van de verwerking van de opeenvolgende jogintenties door GRBL.

De pendant probeert niet zelf de fysieke beweging te simuleren.

# 12. Tijdslot → GRBL

Elke plannerupdate vertegenwoordigt één tijdslot.

Bij:

```text
20 Hz
```

is dat:

```text
50 ms
```

De planner bepaalt hoeveel beweging in dat tijdslot gewenst is.

Deze bewegingsdelta wordt vervolgens vertaald naar een `$J`-commando.

De tijdbasis van het plannerinterval moet daarbij expliciet onderdeel zijn van de vertaling.

Het model is dus niet:

```text
"beweeg X millimeter"
```

maar:

```text
"beweeg deze delta gedurende het volgende tijdsinterval"
```

De feedrate die naar GRBL wordt gestuurd is daarmee een afgeleide van de gewenste beweging binnen de beschikbare tijd.

# 13. MIN_FEEDRATE en MAX_FEEDRATE

De planner gebruikt een minimale en maximale feedrate:

```text
MIN_FEEDRATE
MAX_FEEDRATE
```

Deze grenzen zijn fysieke machinegrenzen, geen compensatie voor oude gebruikersintentie.

Wanneer een gewenste beweging niet sneller kan worden uitgevoerd dan:

```text
MAX_FEEDRATE
```

wordt de intentie niet eindeloos doorgeschoven.

De niet-uitgevoerde intentie blijft immers niet onbeperkt geldig.

Ze valt uiteindelijk aan de achterkant uit de ring.

Dit voorkomt dat een overschrijding van een fysieke grens zich opstapelt tot een steeds groter wordende vertraging.

# 14. Communicatie is niet hetzelfde als intentie

Een belangrijk onderscheid is:

```text
gebruikersintentie
```

versus:

```text
communicatie met CNCjs
```

Een intentie die nog niet naar CNCjs is verstuurd is niet automatisch een "te bewaren commando".

Het systeem mag dus niet redeneren:

```text
"Dit pakketje is nog niet verstuurd,
dus ik bewaar het totdat de verbinding weer tijd heeft."
```

In plaats daarvan geldt:

```text
intentie heeft een tijdspositie

        ↓

tijdslot bereikt zijn moment

        ↓

communiceren indien mogelijk

        ↓

slot verloopt
```

Als communicatie tijdelijk niet beschikbaar is, kan actuele intentie daardoor gewoon verlopen.

Dat is gewenst gedrag voor een interactieve pendant.

# 15. Machinefeedback

Machinefeedback is niet langer de bron van een voorspeld toekomstig target.

`MachineState` vertelt ons:

```text
waar de machine werkelijk is
```

GRBL bepaalt zelf:

```text
hoe de machine beweegt
```

De planner gebruikt feedback daarom vooral om:

* de actuele machinepositie te kennen;
* de volgende intentie te kalibreren;
* te controleren of de machine de verwachte beweging daadwerkelijk volgt;
* afwijkingen, limieten of machinecondities te herkennen.

De machinefeedback is daarmee feedback voor de gebruiker en voor de volgende plannerbeslissing.

Niet:

```text
machinepositie → opnieuw een absolute targetpositie bouwen
```

maar:

```text
machinefeedback
       ↓
volgende intentie beter kalibreren
```

# 16. De ring als bewegend planvenster

Het volledige model kan visueel worden voorgesteld als een ring:

```text
                         TOEKOMST
                            │
                            ▼

                 ┌─────────────────────┐
              ┌──┤ slot 6  slot 7      ├──┐
            ┌─┘  │                     │  └─┐
           │     │ slot 5       slot 8 │    │
           │     │                     │    │
           │     │ slot 4       slot 9 │    │
           │     │                     │    │
            └─┐  │ slot 3  slot 2     │  ┌─┘
              └──┤ slot 1       NU    ├──┘
                 └─────────────────────┘
                            │
                            ▼
                           NET
```

Het planvenster beweegt voortdurend door de ring:

```text
NET → NU → 0,05 s → 0,10 s → ... → 0,50 s
```

Achter `NET` bestaat de intentie niet meer.

Voorbij de maximale planhorizon bestaat de intentie nog niet.

Daarmee geldt:

```text
NIET ─── NET ─── NU ───────────── TOEKOMST ─── NIET
             <------ 0,5 s ------->
```

Dit maakt de ring tegelijkertijd:

* een buffer;
* een tijdmodel;
* een natuurlijke begrenzing van latency;
* een mechanisme om oude intentie te vergeten.

# 17. Architectuur

De gewenste architectuur is:

```text
Encoder Task
      │
      ▼
InputManager
      │
      │ EVENT_ENCODER_PULSE
      │
      ▼
JogPlanner
      │
      ├── actuele gebruikersintentie
      │
      ├── intentiering
      │
      ├── tijdslots
      │
      ├── intentielifetime
      │
      └── gewenste beweging voor volgend tijdslot
      │
      ▼
MachineMapper
      │
      │ $J
      ▼
CNCjs
      │
      ▼
GRBL
      │
      ▼
machine
```

Machinefeedback loopt terug:

```text
GRBL
   ↓
CNCjs
   ↓
MachineState
   ↓
JogPlanner
```

De verantwoordelijkheden zijn daarmee:

```text
JogPlanner

    bepaalt de actuele, tijdgebonden gebruikersintentie.


MachineMapper

    vertaalt een tijdslot naar concrete GRBL/$J-communicatie.


GRBL

    bepaalt hoe de fysieke machine de ontvangen beweging
    accelereert en uitvoert.


MachineState

    rapporteert wat de machine daadwerkelijk doet.
```

# 18. Kernprincipes

Het nieuwe JogPlanner-model kan worden samengevat in een aantal regels:

### 1. Intentie is tijdgebonden

Een gebruikersintentie heeft een beperkte lifetime.

```text
INTENT_LIFETIME = 0,5 s
```

### 2. Intentie is geen command queue

De ring bevat toekomstige bewegingsintentie, niet een rij commando's die gegarandeerd moet worden uitgevoerd.

### 3. De toekomst is beperkt

```text
PLAN_AHEAD = 0,5 s
```

De planner mag nooit onbeperkt vooruit plannen.

### 4. Oude intentie wordt vergeten

Als een slot verlopen is, wordt het geleegd.

Er ontstaat geen backlog.

### 5. Nieuwe intentie past de toekomst aan

Een richtingswisseling wordt onmiddellijk verwerkt in de nog niet verstreken slots.

### 6. De ring is circulair

Vrijgekomen slots worden opnieuw gebruikt voor nieuwe toekomstige intentie.

### 7. De machinepositie is werkelijkheid

De planner houdt geen tweede absolute machinepositie bij die als waarheid kan gaan fungeren.

### 8. GRBL bestuurt de fysieke beweging

De planner bepaalt intentie.

GRBL bepaalt de daadwerkelijke motion.

# Kernmodel

De JogPlanner is uiteindelijk geen planner van toekomstige posities.

Het is een **tijdgebaseerde intentiebuffer**:

```text
              GEBRUIKER
                  │
                  ▼
             encoderinput
                  │
                  ▼
        ┌─────────────────────┐
        │     INTENTIERING    │
        │                     │
        │  [ ][ ][ ][ ][ ]   │
        │  [ ][ ][ ][ ][ ]   │
        │                     │
        │    0 ──── 0,5 s     │
        └─────────┬───────────┘
                  │
                  ▼
             tijdslot
                  │
                  ▼
                $J
                  │
                  ▼
                GRBL
                  │
                  ▼
              MACHINE
                  │
                  ▼
            MachineState
                  │
                  └──────────────►
                       volgende
                       intentie
```

Of nog compacter:

```text
ENCODER

   ↓

ACTUELE INTENTIE

   ↓

10 tijdslots × 50 ms

   ↓

0,5 seconde toekomst

   ↓

$J

   ↓

GRBL

   ↓

FYSIEKE BEWEGING

   ↓

FEEDBACK

   ↓

VOLGENDE INTENTIE
```

Het essentiële ontwerpprincipe is:

> **De pendant plant niet wat de machine over enkele seconden moet doen. Hij beschrijft alleen wat de gebruiker in de komende halve seconde wil dat de machine doet.**

Daardoor blijft de machine altijd dicht bij de actuele menselijke intentie, terwijl GRBL verantwoordelijk blijft voor de daadwerkelijke fysieke beweging.

1. De hoofdgedachte

PendantController wordt de application controller van de hele pendant.

Niet: “de class die het display bestuurt”.

Maar:

Alles wat de pendant als systeem doet, wordt vanuit PendantController gecoördineerd.

Daarmee wordt main.cpp bewust dom.

                    ┌─────────────────────────────┐
                    │      PendantController      │
                    │                             │
                    │       ORCHESTRATOR          │
                    └──────────────┬──────────────┘
                                   │
          ┌────────────────────────┼────────────────────────┐
          │                        │                        │
          ▼                        ▼                        ▼
   ┌─────────────┐         ┌───────────────┐        ┌───────────┐
   │ InputManager│         │ CNCjsInterface│        │  Display  │
   └──────┬──────┘         └───────┬───────┘        └───────────┘
          │                        │
          ▼                        ▼
       Events                 MachineState
                                  │
                                  ▼
                           ┌─────────────┐
                           │ JogPlanner  │
                           └──────┬──────┘
                                  │
                                  ▼
                             JogCommand
                                  │
                                  ▼
                           CNCjsInterface
2. main.cpp wordt bijna alleen composition root

Uiteindelijk wil ik hier ongeveer dit conceptueel overhouden:

main.cpp

setup()
    initialise hardware
    initialise infrastructure
    controller.begin(...)

loop()
    controller.update()

Dus geen:

if(input.available()) ...
jogPlanner.update() ...
cnc.update() ...
machineState = ...
cnc.execute() ...

Dat zijn allemaal systeemregels en horen niet in main.

De serial testcode mag voorlopig een uitzondering blijven, omdat dat testinfrastructuur is en geen pendant-functionaliteit.

3. Wat krijgt PendantController als dependencies?

Ik zou hem expliciet toegang geven tot:

PendantController
    │
    ├── InputManager
    ├── Display
    ├── CNCjsInterface
    └── JogPlanner

Dus conceptueel:

controller.begin(
    input,
    display,
    cnc,
    jogPlanner
);

De MachineState geef je niet meer vanuit main mee.

Waarom?

Omdat MachineState afkomstig is van CNCjsInterface.

De controller kan zelf:

cnc.machineStateSnapshot()

ophalen wanneer hij zijn update uitvoert.

Daarmee verdwijnt deze merkwaardige constructie:

MachineState machineState;

...

machineState =
    cnc.machineStateSnapshot();

uit main.

4. Ownership wordt dan heel duidelijk

Er zijn drie verschillende soorten state.

Pendant state

Eigendom van:

PendantController
        │
        └── PendantState

Bijvoorbeeld:

layer
axis
jogStep

Dit is wat de gebruiker op de pendant heeft geselecteerd.

Machine state

Eigendom van:

CNCjsClientCore
        │
        └── MachineState

Bijvoorbeeld:

machineStatus
machinePosition
workPosition
feedrate
spindleSpeed

Dit is de werkelijkheid van de machine.

De controller gebruikt hiervan snapshots.

Machine settings

Ook afkomstig van CNCjs:

CNCjsClientCore
        │
        └── MachineSettings
                │
                └── maxFeedrate

De controller haalt deze op wanneer dat functioneel nodig is.

Bijvoorbeeld bij:

→ entering LAYER_JOG

en initialiseert daarmee de JogPlanner.

5. De definitieve update() flow

Dit vind ik het belangrijkste onderdeel.

Iedere loop:

PendantController::update()

doet:

┌─────────────────────────────┐
│ 1. Update CNCjs             │
└──────────────┬──────────────┘
               ▼
┌─────────────────────────────┐
│ 2. Snapshot MachineState    │
└──────────────┬──────────────┘
               ▼
┌─────────────────────────────┐
│ 3. Update InputManager      │
└──────────────┬──────────────┘
               ▼
┌─────────────────────────────┐
│ 4. Verwerk input events     │
└──────────────┬──────────────┘
               ▼
┌─────────────────────────────┐
│ 5. Update JogPlanner        │
└──────────────┬──────────────┘
               ▼
┌─────────────────────────────┐
│ 6. Execute JogCommand       │
└──────────────┬──────────────┘
               ▼
┌─────────────────────────────┐
│ 7. Update display            │
└─────────────────────────────┘

Dat maakt de hele applicatieflow op één plek zichtbaar.

6. Input wordt volledig door de Controller gecoördineerd

Nu staat dit nog in main:

input.update()
    ↓
input.read()
    ↓
if encoder pulse
    → JogPlanner
else
    → PendantController

Dat wil ik niet meer.

De Controller wordt:

InputManager
      │
      ▼
 PendantController
      │
      ├── encoder pulse → JogPlanner
      │
      ├── encoder press → layer logic
      │
      └── keys → pendant logic

Dus InputManager detecteert alleen wat er gebeurd is.

Hij bepaalt niet wat de gebeurtenis betekent.

De Controller bepaalt dat.

Dat past ook precies bij onze eerdere beslissing dat de encoder-input via de pendantlogica loopt.

7. JogPlanner blijft bewust dom

Dit is belangrijk.

We maken PendantController niet tot een nieuwe JogPlanner.

De verantwoordelijkheden blijven:

PendantController
"De gebruiker draait de encoder."
        ↓
"Welke as is geselecteerd?"
        ↓
"Welke jogstep is geselecteerd?"
        ↓
"Wij zitten in JOG."
        ↓
JogPlanner.encoder(...)
JogPlanner
"Ik heb een jog-intentie."
        ↓
"Ik plan daar een beweging voor."
        ↓
"Wat is de maximale feedrate?"
        ↓
JogCommand

En daarna:

PendantController
        ↓
cnc.execute(jogCommand)

Dus:

PendantController = WAT / WANNEER
JogPlanner        = HOE JOGGEN
CNCjsInterface    = HOE NAAR CNCjs

Dat is een heel mooie scheiding.

8. Layer transitions krijgen een echte betekenis

Dit is nu extra belangrijk voor het komende werk aan PendantLayers.

We willen niet alleen:

pendantState.layer = LAYER_JOG;

maar conceptueel:

setLayer(LAYER_JOG)
        │
        ├── exit huidige layer
        │
        ├── wijzig state
        │
        ├── enter nieuwe layer
        │
        └── markeer display dirty

Dus:

             setLayer()
                 │
        ┌────────┴────────┐
        ▼                 ▼
   onLayerExit()     onLayerEnter()
                           │
                           ├── JOG
                           │     └── initialise JogPlanner
                           │
                           ├── INFO
                           │
                           └── CONTROL

En hier hoort dus die eerder besproken initialisatie:

ENTER JOG
   ↓
cnc.machineSettingsSnapshot()
   ↓
jogPlanner.begin(settings)
   ↓
jogPlanner.setJogStepDistance(...)

Niet in main.

Niet via een j-commando.

Niet ergens verborgen in JogPlanner.

9. Dat geeft ons een heel duidelijke JOG-flow

Bijvoorbeeld encoder rechts:

Encoder
   ↓
InputManager
   ↓
EVENT_ENCODER_PULSE
   ↓
PendantController
   ↓
layer == JOG?
   ↓ yes
JogPlanner.encoder(
    event,
    pendantState.axis
)
   ↓
JogPlanner intent
   ↓
JogPlanner.update(machineState)
   ↓
JogCommand
   ↓
CNCjsInterface.execute()

En een key:

KEY_6
  ↓
InputManager
  ↓
PendantController
  ↓
jogStep--
  ↓
displayDirty

En encoder press:

ENCODER_PRESS
      ↓
PendantController
      ↓
JOG → INFO
      ↓
layer transition
      ↓
displayDirty
10. Display blijft output

Ook dit zou ik nu definitief vastleggen.

Display weet niets van:

PendantState
MachineState
CNCjs
JogPlanner
input
layers

De Controller vertaalt alles naar display-informatie.

Dus:

MachineState
     ↓
PendantController
     ↓
"ALARM"
"X12.30 Y4.20"
"Z-1.00"
     ↓
Display

en:

PendantState
     ↓
PendantController
     ↓
"JOG"
"Axis X"
"Step 0.10"
     ↓
Display

Dat maakt straks het ontwerpen van de PendantLayers UI veel schoner.

11. De uiteindelijke architectuur

Alles bij elkaar:

                         ┌─────────────────────────┐
                         │          main           │
                         │                         │
                         │  setup()                │
                         │  loop()                 │
                         └────────────┬────────────┘
                                      │
                                      │ update()
                                      ▼
                    ╔═══════════════════════════════╗
                    ║       PendantController       ║
                    ║                               ║
                    ║  PendantState                 ║
                    ║  layer management             ║
                    ║  input routing                ║
                    ║  machine-state coordination   ║
                    ║  jog coordination             ║
                    ║  display coordination         ║
                    ╚═══════════════╤═══════════════╝
                                    │
          ┌─────────────────────────┼─────────────────────────┐
          │                         │                         │
          ▼                         ▼                         ▼
 ┌─────────────────┐       ┌──────────────────┐       ┌──────────────┐
 │  InputManager   │       │ CNCjsInterface   │       │   Display    │
 │                 │       │                  │       │              │
 │ ButtonMatrix    │       │ CNCjsClientCore  │       │ OLED         │
 │ Encoder         │       │ MachineState     │       │              │
 └─────────────────┘       │ MachineSettings  │       └──────────────┘
                           └────────┬─────────┘
                                    │
                                    │ JogCommand
                                    ▼
                           ┌──────────────────┐
                           │    JogPlanner    │
                           │                  │
                           │ intent           │
                           │ feedrate         │
                           │ movement         │
                           └──────────────────┘
En één belangrijke regel

De controller orkestreert, maar bezit niet de verantwoordelijkheden van de andere classes.

Dus niet:

PendantController doet alles.

Maar:

PendantController bepaalt wie wat wanneer doet.

Dat is volgens mij precies de architectuur die we de afgelopen refactors eigenlijk langzaam aan het bereiken waren.

En hiermee hebben we ook een solide basis om nu terug te gaan naar jouw oorspronkelijke doel: de hoofdstructuur van de Pendant en de UI van de PendantLayers.