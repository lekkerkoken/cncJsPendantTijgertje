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


ISS-006 — Data-uitwisseling tussen tasks
Waarschijnlijk queues/state snapshots/andere veilige mechanismen bepalen.

ISS-007 — JogPlanner async maken
Bijvoorbeeld een eigen vaste plannerfrequentie, onafhankelijk van de inputfrequentie.

ISS-008 — Jog cancel afronden
De bestaande CNCjs jogCancel netjes in de uiteindelijke keten opnemen.

ISS-009 — Sequence diagrams/documentatie bijwerken
Nu pas, want we weten inmiddels veel beter hoe de architectuur werkelijk geworden is.