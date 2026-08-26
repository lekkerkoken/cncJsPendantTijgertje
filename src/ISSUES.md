ISS-001 — Encoder als volwaardige InputManager-bron ✅ AFGEROND. — De Encoder wordt de bron van EncoderEvent. Voor ontwikkeling kan een tijdelijke seriële testinput events in de Encoder injecteren. Hierdoor blijft de volledige productieketen intact en hoeft main.cpp geen Event-objecten te manipuleren.

ISS-002 — InputManager coalescing: ✅ AFGEROND.
De fysieke input wordt asynchroon verzameld. Encoderpulsen worden samengevoegd tot één delta voordat ze als InputEvent naar de PendantController worden gestuurd.

Met deze verantwoordelijkheden:

Class	Verantwoordelijkheid
Encoder	Quadrature via ISR + encoder button
ButtonMatrix	Matrix scanning + debounce
InputManager	Input queues lezen + events normaliseren + encoder coalescing
PendantController	InputEvents interpreteren
JogPlanner	Joggedrag uitvoeren

En voorlopig:

geen core pinning.

FreeRTOS mag zelf bepalen waar de tasks draaien.

ISS-003 — Echte encoder testen
De tijdelijke KEY_1 → LEFT / KEY_3 → RIGHT simulatie vervangen zodra de echte encoder beschikbaar is.

ISS-004 — Async inputarchitectuur: ✅ AFGEROND.

De gewenste onafhankelijkheid bestaat al:

✅ ButtonMatrix eigen task
✅ Encoder onafhankelijk
✅ InputManager eigen task
✅ FreeRTOS queues tussen componenten
✅ loop() hoeft input niet te pollen
✅ ButtonMatrix en InputManager zijn losgekoppeld
✅ Controller krijgt alleen abstracte Events


ISS-005 — CNCjsInterface async maken

Status: SPLIT

Doel:

De pendant-loop mag nooit wachten op netwerk-I/O. CNCjsInterface verwerkt netwerkverkeer opportunistisch en levert resultaten/events via een lokale, niet-blokkerende interface aan de rest van het systeem.

Opgesplitst in:

ISS-005a — Connection health ✅ AFGEROND.

Scope:

Socket.IO runtime processing
connection state machine
reconnect
heartbeat
heartbeat timeout
connection health beschikbaar maken
geen heartbeat queue
geen stale commands uitvoeren na reconnect

Acceptance: pendant kan onbeperkt blijven draaien terwijl CNCjs beschikbaar, tijdelijk unavailable of opnieuw verbindend is, zonder dat de hoofdloop bewust op netwerk-I/O wacht.

→ Voldaan.

ISS-005b — Async startup/reconnect

ISS-005b — Async startup/reconnect

Status: OPEN 🔴

Doel

CNCjsInterface mag de hoofdloop van de pendant nooit blokkeren tijdens:

WiFi verbinden
DNS/mDNS resolution
TCP connect
CNCjs authentication
Socket.IO connect
reconnects

De loop() moet altijd responsief blijven.

Architectuur

Netwerkoperaties die potentieel kunnen blokkeren mogen worden uitgevoerd in een aparte FreeRTOS task.

Daarmee wordt de verantwoordelijkheid gescheiden:

Arduino loop()
    │
    ├── Input
    ├── PendantController
    ├── JogPlanner
    ├── MachineMapper
    │
    └── CNCjsInterface.update()
             │
             └── nooit blokkerende netwerkoperatie

De netwerkcommunicatie draait onafhankelijk:

FreeRTOS Network Task
        │
        ├── WiFi
        ├── DNS/mDNS
        ├── TCP
        ├── authentication
        └── Socket.IO

CNCjsInterface vormt de systeemgrens richting CNCjs.

We hoeven binnen deze architectuur dus geen onderscheid te maken tussen de interne transportmechanismen van CNCjs/GRBL. De pendant communiceert met CNCjs als systeemgrens.

Hoofdregel
begin()

begin() mag geen netwerk-I/O uitvoeren.

Het initialiseert uitsluitend:

state
synchronisatie
queues/pending-command state
netwerk-task

Daarna moet begin() onmiddellijk terugkeren.

begin()
   ↓
return
update()

update() mag nooit wachten op netwerk-I/O.

Dus nooit:

update()
   ↓
connect()
   ↓
wachten
   ↓
timeout
   ↓
return

Wel:

update()
   ↓
status/synchronisatie bekijken
   ↓
eventueel kleine lokale actie
   ↓
return

De netwerk-task mag ondertussen blokkeren; de Arduino loop() niet.

Connection state machine

De verbinding wordt beheerd als een asynchrone state machine:

START
  ↓
WIFI CONNECTING
  ↓
RESOLVING
  ↓
AUTHENTICATING
  ↓
SOCKET CONNECTING
  ↓
WAITING FOR LISTS
  ↓
OPENING CONTROLLER
  ↓
READY

Bij een fout:

          ┌──────────────┐
          │              ↓
       READY ← ... ← START
          │
          ↓
       BACKOFF
          │
          └────────────→ START

Alle timeouts worden gebaseerd op timestamps/state, niet op blokkerende netwerkcalls.

Synchronisatie tussen loop en netwerk-task

De netwerk-task en de hoofdloop mogen niet onafhankelijk commands gaan uitvoeren.

Er komt daarom een expliciete synchronisatie tussen:

connection state
connection generation
pending commands
command validity

De hoofdregel is:

Een command dat tijdens een slechte verbinding is ontstaan, mag nooit later automatisch worden uitgevoerd nadat de verbinding opnieuw is opgebouwd — tenzij het command expliciet reconnect-safe is gemaakt.

Connection generation

Iedere succesvolle nieuwe CNCjs-sessie krijgt een nieuwe connection generation.

Normale machinecommando's worden gekoppeld aan de generation waarin ze zijn ontstaan.

Bij reconnect:

generation 17
    ↓
verbinding verloren
    ↓
generation 18

Commands uit generation 17 mogen niet worden uitgevoerd in generation 18.

Dit voorkomt dat bijvoorbeeld een oude:

G91 X0.1 F100

na een reconnect alsnog wordt uitgevoerd.

Dit geldt in het bijzonder voor:

G-code
jog commands
resume
resetachtige machineacties
overige state-afhankelijke commands
Command queue

Er mag geen onbeheerde command queue ontstaan tijdens een verbroken verbinding.

We willen nadrukkelijk geen gedrag zoals:

connection lost

G-code 1
G-code 2
G-code 3
G-code 4
...
        ↓
reconnect
        ↓
alles alsnog uitvoeren

Dat is ongewenst en potentieel gevaarlijk.

Voor normale machinecommando's geldt daarom:

Niet uitvoerbaar tijdens de huidige connection generation → discard.

Er is maximaal één actuele pending machine command waar dat logisch is, zoals de huidige G-code/jog command.

Feed Hold — uitzondering

Feed Hold krijgt bewust een andere semantiek.

Een Feed Hold is geen command dat uitsluitend geldig is binnen dezelfde connection generation.

Reden:

Als Feed Hold nu wordt gevraagd en de verbinding tijdelijk wegvalt, is het nog steeds nuttig om hem kort daarna alsnog uit te voeren.

Daarom is Feed Hold:

time-bound in plaats van generation-bound.

Feed Hold TTL

De geldigheid van Feed Hold is:

4 seconden

De timestamp wordt vastgelegd op het moment waarop de gebruiker de Feed Hold activeert.

Bijvoorbeeld:

t = 10.000
FEED HOLD
expires = 14.000

Reconnect:

t = 10.100
connection lost

t = 12.500
CNCjs READY

t = 12.501
FEED HOLD → SEND

Dit is geldig.

Maar:

t = 10.000
FEED HOLD

...

t = 14.001
CNCjs READY

FEED HOLD → DISCARD

Het command is verlopen.

Belangrijke eigenschap

De 4 seconden beginnen bij user activation, niet bij het moment waarop de netwerk-task het command daadwerkelijk verwerkt.

Command classificatie

We krijgen daarmee conceptueel drie categorieën:

1. Generation-bound commands

Normale machinecommands.

Voorbeelden:

G-code
jog
normale machineacties

Eigenschap:

createdGeneration == currentGeneration

Anders discard.

2. Time-bound commands

Commands die een korte reconnect mogen overleven.

Momenteel:

Feed Hold
TTL = 4 seconden

Eigenschap:

now < expiresAt

Generation hoeft niet gelijk te zijn.

3. Status/query commands

Bijvoorbeeld machine status opvragen.

Deze worden niet gequeued om later uit te voeren.

Als CNCjs niet beschikbaar is:

status request → discard

De volgende heartbeat/query wordt gewoon opnieuw gegenereerd zodra de verbinding READY is.

Socket.IO

Socket.IO wordt eveneens volledig asynchroon behandeld.

De netwerk-task verzorgt:

connect
reconnect
ontvangen events
authentication
CNCjs protocolcommunicatie

De hoofdloop hoeft niet te wachten op Socket.IO.

Een socket disconnect veroorzaakt een state transition en invalidatie van normale pending commands.

Heartbeat

Machine-status wordt periodiek opnieuw opgevraagd.

Een heartbeat is geen queued machinecommand.

Bijvoorbeeld:

READY
  ↓
statusreport
  ↓
GRBL response
  ↓
MachineState heartbeat

Wanneer geen machine-status meer wordt ontvangen:

heartbeat timeout
    ↓
MachineState.connected = false
    ↓
MACHINE_DISCONNECTED

Dit hoeft niet onmiddellijk de volledige Socket.IO-verbinding te verbreken.

Veiligheidsprincipe

De belangrijkste invariant van ISS-005b:

Een reconnect mag nooit leiden tot het automatisch uitvoeren van oude, state-afhankelijke machinecommando's.

De enige expliciete uitzondering is een command dat daar bewust voor ontworpen is.

Op dit moment:

Feed Hold
    ↓
TTL = 4 s
    ↓
mag reconnect overleven
Acceptance criteria
begin()
begin()
    ↓
return onmiddellijk

Geen netwerk-I/O.

loop()
loop()
    ↓
update()
    ↓
return

Geen potentieel blokkerende netwerkoperaties.

Netwerk

WiFi, DNS, TCP en authentication mogen blokkeren, maar uitsluitend in de aparte netwerk-task.

Reconnect

Reconnect gebeurt zonder de Arduino loop() te blokkeren.

Commands

Normale commands uit een oude connection generation worden nooit na reconnect uitgevoerd.

Feed Hold

Feed Hold:

mag reconnect overleven;
heeft een TTL van 4 seconden;
wordt uitgevoerd zodra CNCjs weer beschikbaar is, indien nog geldig;
wordt na 4 seconden weggegooid.
Queue

Er kan nooit een backlog van commands ontstaan die na reconnect wordt afgespeeld.

Status

De hoofdloop kan te allen tijde de laatst gesynchroniseerde CNCjs/connection status uitlezen.

Ontwerpbeslissing

Goedgekeurd ontwerp:

FreeRTOS network task + non-blocking CNCjsInterface.update() + connection generation voor normale commands + expliciete TTL-based uitzondering voor Feed Hold van 4 seconden.

# ISS-005c — opgelost met de huidige implementatie. ✅
Probleem

Sinds het CNCjs-netwerkwerk in een eigen FreeRTOS-task draait, kunnen netwerkoperaties vanuit de applicatielogica (loop()) gelijktijdig plaatsvinden met netwerkverwerking in de CNCjsNetwork task.

Niet alle toegang tot CNCjsClientCore / SocketIOclient verloopt momenteel via networkMutex_. Daarnaast wordt MachineState vanuit de netwerk-task bijgewerkt terwijl deze vanuit de hoofdloop wordt gelezen.

Voorbeelden zijn onder andere:

selectController() → openSelectedController() → socketIO.sendEVENT()
openController() → socketIO.sendEVENT()
lezen van MachineState vanuit loop()
schrijven naar MachineState vanuit de netwerk-task

Doel

De grens tussen de applicatielogica en de CNCjs-network-task expliciet en thread-safe maken, zonder de verantwoordelijkheden van de bestaande componenten opnieuw te vermengen.

Acceptatiecriteria

Alle toegang tot SocketIOclient vanuit buiten de network-task is thread-safe.
MachineState kan veilig door de network-task worden bijgewerkt en door de applicatielogica worden gelezen.
Controllerselectie en controller-openen blijven betrouwbaar werken.
Jog-commando's blijven zonder merkbare vertraging werken.
Er ontstaan geen deadlocks of blokkeringen van de hoofdloop.
De netwerk-task behoudt zijn zelfstandige verantwoordelijkheid voor CNCjs-netwerkverkeer.

# ISS-005d — Jogging correct en voorspelbaar maken

**Status: 🔵 POSTPONED — bewust uitgesteld; niet vergeten, maar nu geen actie.

## Doel

Het joggen van de pendant moet zich correct en voorspelbaar gedragen.

De basisroute van encoder naar CNCjs werkt inmiddels: een encoderbeweging kan een jog uitvoeren en de machinepositie verandert overeenkomstig.

Het daadwerkelijke joggedrag is echter nog niet volledig correct en moet verder worden onderzocht en aangepast.

## Huidige observatie

Een test met een jog-increment van `0.1 mm` laat bijvoorbeeld zien:

```text
positie 0.0
    ↓
jog +0.1
    ↓
positie 0.1
    ↓
jog -0.1
    ↓
positie 0.0
```

Deze eenvoudige beweging werkt.

Daarmee is vastgesteld dat:

```text
Encoder
   ↓
InputManager
   ↓
JogPlanner
   ↓
MachineMapper
   ↓
CNCjsInterface
   ↓
CNCjs
   ↓
GRBL
```

functioneel met elkaar verbonden zijn.

Het volledige joggedrag moet echter nog worden gevalideerd en gecorrigeerd.

## Te onderzoeken gedrag

Onder andere:

* opeenvolgende encoderbewegingen;
* snel achter elkaar draaien van de encoder;
* positieve en negatieve richting;
* verschillende jog-incrementen;
* grotere verplaatsingen;
* het correct blijven volgen van de actuele machinepositie;
* gedrag wanneer CNCjs tijdelijk niet READY is;
* gedrag bij het wisselen van richting tijdens het joggen.

## Belangrijke ontwerpgrens

De bestaande beslissing over het **aggregeren van jog-bewegingen** staat niet ter discussie in dit issue.

De aggregatie is eerder ontworpen en blijft uitgangspunt voor de verdere implementatie.

Dit issue gaat uitsluitend over het feitelijke gedrag en de correcte uitvoering van joggen binnen die bestaande architectuur.

## Doelgedrag

Een encoderbeweging moet uiteindelijk leiden tot een voorspelbare fysieke machinebeweging waarbij:

```text
encoder input
    ↓
jog planning
    ↓
jog command
    ↓
CNCjs
    ↓
machine
```

de gewenste relatieve verplaatsing wordt uitgevoerd.

De positie die vervolgens via CNCjs/GRBL terugkomt in `MachineState` moet daarmee consistent zijn.

## Acceptance criteria

### Basis

* `+0.1 mm` resulteert in een verplaatsing van `+0.1 mm`.
* `-0.1 mm` resulteert in een verplaatsing van `-0.1 mm`.
* Positieve en negatieve bewegingen kunnen elkaar correct opheffen.

### Opeenvolgende bewegingen

* Meerdere encoderstappen worden correct verwerkt.
* Snel opeenvolgende encoderbewegingen leiden niet tot verloren of onverwachte bewegingen.
* Richtingswisselingen gedragen zich voorspelbaar.

### Incrementen

* Alle ondersteunde jog-incrementen gedragen zich correct.
* De uiteindelijke verplaatsing komt overeen met het gekozen increment en de hoeveelheid encoderinput.

### MachineState

* De door CNCjs teruggegeven machinepositie blijft de leidende werkelijkheid.
* Jogplanning gebruikt geen verouderde positie als actuele machinepositie.

### CNCjs

* Jogcommando's worden correct naar CNCjs gestuurd.
* Het joggedrag blijft correct nadat de CNCjs-network-task uit ISS-005b is geïsoleerd.

## Ontwerpbeslissing

De bestaande architectuur en jog-command-aggregatie blijven behouden.

Eerst wordt het huidige gedrag reproduceerbaar vastgesteld. Daarna wordt bepaald waar in:

```text
Encoder → InputManager → JogPlanner → MachineMapper → CNCjsInterface
```

het afwijkende gedrag ontstaat.

Pas daarna wordt de implementatie aangepast.

ISS-006 — Data-uitwisseling tussen tasks
Waarschijnlijk queues/state snapshots/andere veilige mechanismen bepalen.

ISS-007 — JogPlanner async maken
Bijvoorbeeld een eigen vaste plannerfrequentie, onafhankelijk van de inputfrequentie.

ISS-008 — Jog cancel afronden
De bestaande CNCjs jogCancel netjes in de uiteindelijke keten opnemen.

ISS-009 — Sequence diagrams/documentatie bijwerken
Nu pas, want we weten inmiddels veel beter hoe de architectuur werkelijk geworden is.

ISS-010 - ## Probleem

Joggen via de pendant werkt gedeeltelijk, maar het joggedrag is nog niet correct.

Op dit moment kan de machine bijvoorbeeld:

* `0.1 mm` in positieve richting joggen;
* daarna weer `0.1 mm` terug joggen naar de oorspronkelijke positie.

De basisroute van een jog-command naar CNCjs werkt daarmee, maar het volledige joggedrag moet nog worden onderzocht en gecorrigeerd.

## Doel

Jogging moet zich voorspelbaar gedragen bij:

* meerdere opeenvolgende encoderbewegingen;
* positieve en negatieve richting;
* verschillende jog-incrementen;
* snel achter elkaar draaien van de encoder;
* het bereiken van de gewenste doelpositie.

De pendant moet daarbij de door de gebruiker gevraagde beweging correct vertalen naar CNCjs-jogcommando's.

## Opmerking

Dit issue staat los van Commit 5.

Commit 5 heeft als doel gehad om het CNCjs-netwerkwerk naar een eigen FreeRTOS-task te verplaatsen. Dat is functioneel getest en werkt.

Het huidige jogprobleem moet daarom in een afzonderlijke stap worden onderzocht.

# Issue 11 — Heartbeat alleen bij inactiviteit

## Doel

De CNCjs-heartbeat moet niet langer periodiek `statusreport`-requests sturen terwijl er al actief communicatieverkeer met CNCjs plaatsvindt.

Tijdens een jog ontvangt de pendant voortdurend informatie van CNCjs. Die inkomende events zijn feitelijk al een bewijs dat de verbinding actief is. Een extra heartbeat-request is dan redundant en kan bovendien onnodig concurreren met de jogcommunicatie.

De heartbeat moet daarom worden veranderd van een **periodieke activiteit** naar een **watchdog bij inactiviteit**.

---

## Huidige situatie

De CNCjs-communicatie verstuurt momenteel periodiek een heartbeat, bijvoorbeeld:

```text
[CNCjs] HEARTBEAT: ["command","/dev/ttyUSB0","statusreport"]
```

Dit gebeurt ook wanneer de pendant actief aan het joggen is.

Tijdens zo'n jog komen echter al events binnen zoals:

```text
["serialport:read","ok"]
```

```text
["serialport:write","G91 X0.100 F100\n", ...]
```

```text
["controller:state","Grbl", ...]
```

```text
["Grbl:state", ...]
```

Deze events bewijzen dat de Socket.IO-verbinding en CNCjs-communicatie functioneren.

---

## Gewenst gedrag

Alle relevante inkomende CNCjs-events moeten intern worden beschouwd als **activiteit / heartbeat**.

Bij ontvangst van een geldig CNCjs-event wordt bijvoorbeeld:

```text
lastCncjsActivity = millis()
```

bijgewerkt.

De heartbeat-request wordt vervolgens alleen verstuurd wanneer er gedurende een bepaalde periode géén CNCjs-activiteit is geweest.

Bijvoorbeeld:

```text
CNCjs event
    ↓
lastActivity = now

...

geen events gedurende 1 seconde
    ↓
heartbeat request
    ↓
statusreport
```

Ontvangt de pendant ondertussen opnieuw een CNCjs-event, dan wordt de watchdog opnieuw gereset.

---

## Belangrijk onderscheid

De heartbeat is **geen onderdeel van de jogbesturing**.

`JogPlanner` hoeft niets van heartbeat, Socket.IO of verbindingbewaking te weten.

De verantwoordelijkheid ligt volledig bij de CNCjs-communicatielaag:

```text
JogPlanner
    │
    │ MachineCommand
    ▼
CNCjsClientCore / CNCjsInterface
    │
    ├── normale TX
    ├── normale RX
    │       └── update lastActivity
    │
    └── heartbeat watchdog
            └── alleen request bij langdurige stilte
```

---

## Gewenste eigenschappen

### 1. Joggen onderdrukt de heartbeat niet expliciet

Er hoeft geen speciale code te komen zoals:

```cpp
if(jogging)
    disableHeartbeat();
```

Dat zou de verkeerde abstractie zijn.

De reden dat tijdens een jog geen heartbeat nodig is, is simpelweg dat er al CNCjs-verkeer plaatsvindt.

---

### 2. Iedere relevante inkomende CNCjs-event telt als activiteit

Bijvoorbeeld:

* `serialport:read`
* `serialport:write`
* `controller:state`
* `Grbl:state`
* `feeder:status`
* andere geldige CNCjs-events

Deze hoeven niet allemaal afzonderlijk semantisch als heartbeat geïnterpreteerd te worden. Voor de verbindingswatchdog is alleen van belang:

> Er is recent geldige communicatie van CNCjs ontvangen.

---

### 3. Heartbeat alleen als vangnet

De heartbeat-request moet pas worden verzonden nadat de verbinding gedurende bijvoorbeeld **1 seconde** stil is geweest.

Dit voorkomt:

* onnodige `statusreport` requests tijdens joggen;
* extra verkeer op de Socket.IO-verbinding;
* mogelijke timinginteractie met jogcommando's;
* onnodige belasting van CNCjs;
* een heartbeat die feitelijk dubbelop is met bestaande communicatie.

---

## Voorbeeld

### Actief joggen

```text
JogCommand
    ↓
CNCjs TX: G91 X0.100 F100

CNCjs RX: serialport:write
CNCjs RX: serialport:read "ok"
CNCjs RX: controller:state
CNCjs RX: Grbl:state
CNCjs RX: serialport:read status
...
```

Zolang deze events blijven binnenkomen:

```text
geen heartbeat-request
```

---

### Geen jog, maar verbinding actief

```text
CNCjs RX
    ↓
lastActivity = now

... 300 ms ...

geen event

... 700 ms ...

nog steeds geen event
```

Nog steeds geen heartbeat nodig totdat de ingestelde timeout bereikt is.

---

### Verbinding mogelijk stilgevallen

```text
lastActivity
    │
    └── > 1 seconde geleden
             ↓
        statusreport
             ↓
        CNCjs response
             ↓
        lastActivity = now
```

Wanneer daarop geen antwoord komt, kan de bestaande disconnect/offline-detectie zijn werk doen.

---

## Acceptatiecriteria

* [ ] Tijdens actief joggen wordt geen periodieke heartbeat-request meer verstuurd zolang CNCjs-events binnenkomen.
* [ ] Inkomende CNCjs-events resetten de heartbeat-watchdog.
* [ ] Een heartbeat-request wordt alleen verstuurd na de ingestelde periode van volledige inactiviteit.
* [ ] De heartbeat-logica blijft volledig buiten `JogPlanner`.
* [ ] De bestaande CNCjs-communicatie en authenticatie blijven ongewijzigd.
* [ ] Een actieve jog blijft volledig onafhankelijk van de heartbeat-watchdog functioneren.
* [ ] Bij wegvallen van CNCjs-verkeer kan de bestaande offline-detectie nog steeds optreden.
* [ ] Een normale CNCjs-response op een heartbeat geldt eveneens als nieuwe activiteit.
* [ ] Er worden geen extra `statusreport` requests gegenereerd zolang de verbinding aantoonbaar actief communiceert.

## Opmerking

Dit issue is nadrukkelijk een **communicatie-/watchdogverbetering**. De eerder aangepaste jogplanning en encoderverwerking worden hierbij niet opnieuw ontworpen.

De kern is:

> **Niet periodiek vragen of CNCjs nog leeft wanneer CNCjs ondertussen uit zichzelf tegen ons praat.**
>
> De heartbeat is alleen nodig wanneer het stil wordt.
