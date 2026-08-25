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
NetworkManager

NetworkManager is responsible for network-level services only:

WiFi connection
DNS resolution
HTTP communication
HTTP authentication
obtaining the CNCjs authentication token

NetworkManager does not know about CNCjs protocol messages, Socket.IO events, controllers or machine state.

CNCjsClientCore

CNCjsClientCore is the implementation of the CNCjs client.

It is responsible for:

the CNCjs connection state machine
Socket.IO communication
CNCjs protocol and event handling
controller and serial-port discovery
controller selection and opening
the CNCjs heartbeat
processing CNCjs machine-state information
updating MachineState
executing and transmitting machine commands and G-code
maintaining the internal CNCjs state used by the interface

The network processing runs in the CNCjsClientCore FreeRTOS network task.

CNCjsClientCore uses NetworkManager for WiFi, DNS and HTTP authentication, but does not implement those services itself.

CNCjsInterface

CNCjsInterface is the application-facing boundary of the CNCjs client.

The rest of the application communicates with CNCjs through this interface and should not depend directly on the implementation details of CNCjsClientCore.

The interface is responsible for:

exposing the CNCjs functionality required by the application
providing a stable application-facing API
exposing connection, controller and machine status
providing a consistent snapshot of the current CNCjs state
forwarding application commands to CNCjsClientCore

CNCjsInterface does not implement:

Socket.IO
WiFi
DNS
HTTP authentication
CNCjs protocol parsing
the network task
CNCjs connection handling
State and synchronization

CNCjsClientCore owns and updates the live CNCjs state.

The application consumes that state through CNCjsInterface as a consistent snapshot rather than depending on individual internal Core fields.

The network task and application-facing access are synchronized using the CNCjs network mutex. The synchronization boundary must be preserved when the implementation is refactored.

Architectural rule

The separation is intentional:

NetworkManager provides network services.
CNCjsClientCore implements CNCjs.
CNCjsInterface provides the application API.

New functionality should be placed according to these responsibilities rather than simply added to the class that happens to be easiest to access.

# JogPlanner – ontwerpmodel

## Doel

De JogPlanner vertaalt de menselijke interactie met de encoder naar een continue bewegingsintentie voor de CNC-machine.

De encoder wordt niet gezien als een knop die losse jogcommando's produceert. De gebruiker geeft met de encoder voortdurend aan:

* hoeveel de gewenste beweging verandert;
* in welke richting;
* en hoe snel de bewegingsintentie verandert.

De machinefeedback wordt gebruikt om deze intentie voortdurend bij te stellen.

---

## 1. De gebruiker creëert een bewegingshorizon

De belangrijkste interne toestand van de JogPlanner is de **gebruikershorizon**.

Dit is de positie waar de gebruiker wil dat de machine uiteindelijk uitkomt.

Bijvoorbeeld:

```text
machinepositie = 100 mm

gebruiker draait +10 pulsen

step size = 0,1 mm

horizon = 101 mm
```

Wanneer de machine nog onderweg is en de gebruiker nogmaals +20 pulsen geeft:

```text
horizon = 103 mm
```

De tweede beweging wordt dus niet als een tweede jogcommando in een queue gezet. De bestaande gebruikersintentie wordt uitgebreid.

Bij een verandering van richting kan de horizon eveneens teruglopen.

---

## 2. De encoder verandert de horizon

De encoder heeft 30 pulsen per omwenteling.

Bij een maximale menselijke draaisnelheid van ongeveer 2 omwentelingen per seconde ontstaan maximaal ongeveer 60 pulsen per seconde (ongeveer één puls per 17 ms).

Iedere puls verandert de gebruikershorizon met:

```text
pulse × stepSize
```

Bijvoorbeeld:

```text
+1 puls → +0,1 mm

+1 puls → +0,1 mm

-1 puls → -0,1 mm
```

De encoderfrequentie bepaalt daarmee niet rechtstreeks de feedrate.

De encoderfrequentie vertelt vooral hoe snel de gebruiker zijn bewegingshorizon verandert.

Door de coalescing in `InputManager` kan de JogPlanner meerdere snel binnengekomen fysieke pulsen als één wijziging van de gebruikershorizon ontvangen.

---

## 3. De gewenste aankomsttijd

De gebruiker wil bij een grote beweging niet noodzakelijk een bepaalde feedrate.

De gebruiker wil impliciet:

```text
"Breng mij snel naar de horizon."
```

Daarom gebruiken we voorlopig een gewenste maximale aankomsttijd van:

```text
0,8 seconde
```

De feedrate wordt vervolgens afgeleid uit:

```text
resterende afstand
------------------
gewenste aankomsttijd
```

Bijvoorbeeld:

```text
machinepositie = 100 mm

horizon = 120 mm

resterende afstand = 20 mm

gewenste aankomsttijd = 0,8 s
```

dan is de benodigde gemiddelde snelheid:

```text
20 / 0,8 = 25 mm/s
```

oftewel:

```text
1500 mm/min
```

---

## 4. De feedrate volgt de bewegingshorizon

Wanneer de gebruiker de horizon verder weg schuift, neemt de benodigde feedrate toe.

Bijvoorbeeld:

```text
machine = 100 mm

horizon = 120 mm

afstand = 20 mm

F ≈ 1500 mm/min
```

De gebruiker geeft vervolgens opnieuw input:

```text
horizon = 130 mm
```

dan:

```text
machine = 100 mm

horizon = 130 mm

afstand = 30 mm

F ≈ 2250 mm/min
```

De machine versnelt dus omdat de horizon verder weg komt te liggen, terwijl de gewenste aankomsttijd ongeveer gelijk blijft.

---

## 5. Als de gebruiker stopt

Wanneer de gebruiker stopt met draaien, verandert de horizon niet meer.

De machine beweegt vervolgens richting de bestaande horizon.

Bijvoorbeeld:

```text
horizon = 130 mm

machine:

100 → 105 → 112 → 120 → 127 → 130 mm
```

De resterende afstand wordt:

```text
30 → 25 → 18 → 10 → 3 → 0 mm
```

De benodigde feedrate kan daardoor automatisch afnemen.

De planner hoeft dus niet te wachten op een expliciete "stop"-actie van de gebruiker.

---

## 6. Continue gebruikersintentie

Een menselijke encoderbeweging wordt beschouwd als een continue interactie.

Bijvoorbeeld:

```text
ZWIEP 1

    ↓

horizon +20 mm

machine is onderweg

ZWIEP 2

    ↓

horizon +30 mm
```

De planner maakt daar:

```text
oorspronkelijke horizon + 50 mm
```

van.

Er ontstaat geen queue van:

```text
+20 mm

+30 mm
```

De horizon wordt voortdurend opnieuw berekend op basis van de gebruikersinput en de werkelijke machinepositie.

---

## 7. Richtingsverandering

Wanneer de gebruiker tijdens een beweging terugdraait, wordt de bestaande intentie aangepast.

Bijvoorbeeld:

```text
horizon = +100 mm
```

De gebruiker draait terug:

```text
-20 mm
```

dan:

```text
horizon = +80 mm
```

De planner hoeft dus niet eerst +100 mm uit te voeren en daarna -20 mm.

De gebruiker heeft zijn intentie gewijzigd.

De werkelijke machinepositie blijft afkomstig uit MachineState.

---

## 8. Bewegingshorizon en aankomsttijd

De bewegingshorizon is dus primair.

De feedrate is daarvan afgeleid.

Conceptueel:

```text
encoder input

      ↓

verandering horizon

      ↓

gebruikershorizon

      ↓

machinepositie uit MachineState

      ↓

resterende afstand

      ↓

gewenste aankomsttijd ≈ 0,8 s

      ↓

benodigde feedrate
```

De causaliteit is dus:

```text
gebruiker → horizon → aankomsttijd → feedrate
```

en niet:

```text
gebruiker → feedrate → afstand
```

---

## 9. Machinefeedback

De JogPlanner gebruikt MachineState om te bepalen waar de machine daadwerkelijk is.

Belangrijke informatie is onder andere:

* machinepositie;
* werkpositie;
* machine velocity / feedrate;
* machine status;
* alarm / error state.

De planner moet onderscheid maken tussen:

```text
gewenste positie

werkelijke positie
```

De gewenste positie is de gebruikershorizon.

De werkelijke positie komt van de machine.

---

## 10. Plannerfrequentie

De JogPlanner werkt voorlopig op:

```text
20 Hz

1 update per 50 ms
```

De encoderinput zelf kan veel sneller binnenkomen.

20 Hz is dus de frequentie waarmee de planner zijn toestand opnieuw beoordeelt en eventueel nieuwe communicatie naar CNCjs genereert.

De uiteindelijke optimale frequentie hangt mede af van:

* CNCjs;
* beschikbare buffers;
* TinyG/GRBL;
* motion planner;
* machine acceleratie;
* maximale machinesnelheid.

---

## 11. Feedrategrenzen

Voorlopig gebruiken we:

```text
minimum = 100 mm/min

maximum = 3000 mm/min
```

Deze waarden zijn voorlopig.

Idealiter worden deze grenzen later uit de machineconfiguratie of machine capabilities gehaald.

De berekende feedrate is de feedrate die nodig is om de huidige bewegingshorizon binnen de gewenste aankomsttijd te bereiken.

---

## 12. Relatieve jogcommando's

Voor de communicatie met de motion controller gebruiken we bij voorkeur relatieve jogbewegingen:

```text
$J=G91 X10 F1200
```

Dit betekent:

```text
beweeg vanaf de huidige positie 10 mm in X

met een feedrate van 1200 mm/min
```

De JogPlanner werkt echter intern met een gebruikershorizon.

De uiteindelijke vertaling van:

```text
gebruikershorizon

+

werkelijke machinepositie

+

gewenste feedrate
```

naar concrete `$J=`-commando's wordt verzorgd door de MachineMapper/communicatielaag.

---

## 13. Geen command queue voor gebruikersintentie

De JogPlanner behandelt encoderinput als wijzigingen aan één bewegingsintentie.

Dus niet:

```text
queue:

    +100

    +20

    -10
```

maar:

```text
horizon:

    +100

    → +120

    → +110
```

De machine kan ondertussen al onderweg zijn.

Daarom is MachineState essentieel: de planner moet weten hoeveel van de intentie inmiddels daadwerkelijk gerealiseerd is.

De inputqueue is dus geen queue van geplande machinebewegingen.

De inputqueue transporteert alleen nog niet verwerkte gebruikersinput.

Na coalescing in `InputManager` wordt deze input vertaald naar een verandering van de gebruikershorizon.

---

## 14. Architectuur

De gewenste architectuur is:

```text
Encoder Task

   ↓

InputManager

   ↓

EVENT_ENCODER_PULSE

   value = aantal samengevoegde pulsen

   ↓

JogPlanner

   │

   ├── gebruikershorizon

   ├── encoderfrequentie

   ├── resterende afstand

   ├── gewenste aankomsttijd

   └── gewenste feedrate

   ↓

MachineMapper

   ↓

CNCjs

   ↓

TinyG / GRBL

   ↓

machine
```

Machinefeedback loopt terug:

```text
TinyG

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

    bepaalt WAT de gebruiker wil.

MachineMapper

    bepaalt HOE deze intentie wordt vertaald naar CNCjs/TinyG.

TinyG

    bepaalt HOE de machine fysiek accelereert en beweegt.
```

---

## Kernmodel

De JogPlanner kan uiteindelijk worden samengevat als:

```text
Encoder

   ↓

"waar wil de gebruiker heen?"

   ↓

Horizon

   ↓

"waar is de machine nu?"

   ↓

Resterende afstand

   ↓

"wanneer wil de gebruiker daar zijn?"

   ↓

≈ 0,8 seconde

   ↓

Feedrate

   ↓

Jogbeweging
```

De 0,8 seconde is geen maximale afstand.

Het is een gewenste tijdshorizon voor de aankomst.

De afstand die binnen die tijd wordt afgelegd is afhankelijk van de resterende afstand en daarmee van de gebruikersintentie.