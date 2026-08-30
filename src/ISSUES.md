ISS-006 — Pendant timing en concurrency onderzoeken
Doel

De pendant moet betrouwbaar en voorspelbaar reageren op input en jog-opdrachten.

Op dit moment is joggen nog steeds niet betrouwbaar. Dezelfde gebruikersactie levert niet altijd hetzelfde gedrag op. Dit kan wijzen op timingproblemen tussen de verschillende asynchrone onderdelen van de pendant, bijvoorbeeld inputverwerking, jogplanning, CNCjs-communicatie en het ontvangen/verwerken van machine-status.

Het ontbreken van de definitieve antenne is mogelijk van invloed op de WiFi-prestaties, maar mag niet zonder onderzoek als oorzaak worden aangenomen.

Observatie

De basisfunctionaliteit werkt:

encoder-events worden ontvangen;
de pendant-controller verwerkt de events;
de JogPlanner wordt aangestuurd;
CNCjs ontvangt jog-opdrachten;
de machine beweegt.

Toch is het joggen niet betrouwbaar en/of voorspelbaar.

Dit suggereert dat het probleem mogelijk niet meer in de functionele keten zelf zit, maar in de timing en onderlinge synchronisatie van de activiteiten.

Mogelijke oorzaken

Te onderzoeken:

timing van de FreeRTOS-taken;
prioriteiten van de verschillende taken;
blocking calls;
timing van WiFi- en Socket.IO-communicatie;
timing tussen encoder-input en JogPlanner;
timing van machine-state updates;
timing van het versturen van opeenvolgende jog-opdrachten;
race conditions tussen taken;
queuegedrag en eventuele queue-overflow;
het coalescen van encoder-pulsen;
vertraging/jitter in de WiFi-verbinding;
invloed van het ontbreken van de antenne;
eventuele vertraging doordat meerdere onderdelen in dezelfde task draaien.
Onderzoek

Voordat functionaliteit verder wordt aangepast, eerst inzicht krijgen in de timing.

Voor relevante gebeurtenissen timestamps/logging toevoegen, bijvoorbeeld:

encoder-puls ontstaat;
encoder-event wordt door InputManager ontvangen;
event wordt gepubliceerd;
PendantController verwerkt het event;
JogPlanner ontvangt/verwerkt de verandering;
JogPlanner genereert een jog;
CNCjsClient verstuurt de opdracht;
CNCjs ontvangt de opdracht;
machine-state verandert;
nieuwe machine-state komt terug bij de pendant.

Daarmee moet zichtbaar worden waar de vertraging of variatie ontstaat.

Bij voorkeur worden hierbij timestamps in milliseconden gebruikt, zodat één jogactie door de volledige keten gevolgd kan worden.

Belangrijke onderzoeksvraag

Is het jogprobleem werkelijk een probleem met de joglogica, of is de joglogica op zichzelf correct maar wordt deze door timing/concurrency niet betrouwbaar uitgevoerd?

Acceptatiecriteria
De timing van de volledige input → jog → machine-state keten is inzichtelijk.
Er is vastgesteld welke taken/events gelijktijdig kunnen optreden.
Eventuele race conditions of blocking calls zijn geïdentificeerd.
De invloed van WiFi/antenne is afzonderlijk getest.
Een encoderbeweging resulteert onder dezelfde omstandigheden voorspelbaar in hetzelfde joggedrag.
Pas daarna wordt de JogPlanner of andere functionele logica verder aangepast als dat daadwerkelijk nodig blijkt.
Relatie met ISS-005d

ISS-005d — Jogging correct en voorspelbaar maken blijft het functionele issue.

ISS-006 onderzoekt specifiek of timing, concurrency en communicatievertraging de oorz

ISS-005e — Jogstep centraal gebruiken

Status: OPEN

Doel:
De geselecteerde jogstep van de pendant moet één centrale bron van waarheid hebben en daadwerkelijk door alle relevante onderdelen van de pendant worden gebruikt.

Huidige situatie:

PendantState bevat de jogstep als enum:

enum JogStep
{
    STEP_10_MM,
    STEP_1_MM,
    STEP_0_1_MM,
    STEP_0_01_MM
};

De daadwerkelijke waarde wordt echter op meerdere plaatsen genegeerd en hardcoded.

1. JogPlanner

JogPlanner gebruikt momenteel:

static constexpr float STEP_SIZE = 0.1f;

De geselecteerde PendantState::JogStep heeft hierdoor geen invloed op de daadwerkelijke jogafstand.

2. PendantController

Ook de weergave van de jogstep is hardcoded:

display->setLine2(
    "Step 0.10"
);

Hierdoor kan het display een andere waarde tonen dan de daadwerkelijk geselecteerde jogstep.

Gewenst gedrag:

PendantState::JogStep is de single source of truth voor de geselecteerde jogstep.
Er bestaan geen hardcoded jogstep-waarden meer in PendantController of JogPlanner.
PendantController toont de daadwerkelijk geselecteerde jogstep op het display.
JogPlanner gebruikt de daadwerkelijk geselecteerde jogstep voor nieuwe jogbewegingen.
De enum wordt centraal vertaald naar de bijbehorende afstand:
STEP_10_MM → 10.0 mm
STEP_1_MM → 1.0 mm
STEP_0_1_MM → 0.1 mm
STEP_0_01_MM → 0.01 mm
De keuze van de jogstep blijft een verantwoordelijkheid van de pendant/state; JogPlanner hoeft niet rechtstreeks afhankelijk te worden van PendantState.
De bestaande jog-planning, horizon en timing blijven verder ongewijzigd.

Acceptatiecriteria:

Wanneer de gebruiker de jogstep op de pendant verandert, toont het display de nieuwe waarde én gebruiken nieuwe jogbewegingen dezelfde waarde.

Dus bijvoorbeeld:

PendantState
    │
    │ JogStep
    ▼
PendantController ──────► Display
    │
    │ JogStep
    ▼
JogPlanner ─────────────► daadwerkelijke jogafstand

Niet onderdeel van dit issue:

Heartbeat/statusreport timing
Jog timing
Horizon/planning-algoritme
Encoderverwerking
Asselectie

Dit maakt ook meteen duidelijk waarom dit een issue is: niet alleen de planner is fout, maar de JogStep uit PendantState is momenteel feitelijk geen echte statebron.

ISS-005f — Jogstep selecteren met keys

Status: OPEN

Doel:

De gebruiker moet vanaf de pendant de geselecteerde jogstep kunnen aanpassen met twee keys.

Gewenst gedrag:

KEY_6 vergroot de geselecteerde jogstep met één niveau.
KEY_9 verkleint de geselecteerde jogstep met één niveau.
De nieuwe selectie wordt opgeslagen in PendantState::jogStep.
Het display toont na wijziging direct de nieuwe jogstep.
Nieuwe jogbewegingen gebruiken automatisch de nieuwe jogstep.
De bestaande centrale vertaling JogStep → mm blijft de enige bron voor de daadwerkelijke afstand.

De beschikbare jogsteps zijn, van groot naar klein:

STEP_10_MM
STEP_1_MM
STEP_0_1_MM
STEP_0_01_MM

Dus:

KEY 6                           KEY 9
  │                               │
  ▼                               ▼
groter                          kleiner

0.01 ──► 0.1 ──► 1.0 ──► 10.0 mm
       ◄───────────────

Grenzen:

Bij STEP_10_MM doet KEY_6 niets.
Bij STEP_0_01_MM doet KEY_9 niets.
Er wordt niet doorgelopen van de grootste naar de kleinste waarde of andersom.

Architectuur:

PendantController is verantwoordelijk voor het wijzigen van de geselecteerde jogstep, omdat de keuze van de jogstep onderdeel is van de pendant/state.

KEY_6 / KEY_9
      │
      ▼
PendantController
      │
      ▼
PendantState.jogStep
      │
      ├──────────────► Display
      │
      ▼
jogStepDistance()
      │
      ▼
JogPlanner

JogPlanner krijgt geen kennis van de keys en blijft onafhankelijk van PendantState.

Niet onderdeel van dit issue:

Nieuwe jogsteps toevoegen.
De bestaande JogStep-enum wijzigen.
Wijzigingen aan de centrale JogStep → mm-vertaling.
Encoderverwerking wijzigen.
Asselectie wijzigen.
Jog-planning, horizon of timing wijzigen.
Display-layout wijzigen.

Acceptatiecriteria:

Wanneer de gebruiker op key 6 drukt, wordt de jogstep één niveau groter. Wanneer de gebruiker op key 9 drukt, wordt deze één niveau kleiner. Het display toont de nieuwe waarde en de eerstvolgende nieuwe jogbeweging gebruikt dezelfde waarde.

Voorbeeld:

Bij startup:

Step 1.00

Na KEY_6:

Step 10.00

Nogmaals KEY_6:

Step 10.00

Na KEY_9:

Step 1.00

Daarna:

KEY_9
↓
Step 0.10
↓
Step 0.01
↓
Step 0.01

## ISSUE 6 Max feedrate opslaan in MachineState

### Doel

De maximale feedrate van de CNC-machine beschikbaar maken via `MachineState`.

Voor een Grbl-controller zijn de relevante instellingen:

* `$110` — maximale X-feedrate
* `$111` — maximale Y-feedrate
* `$112` — maximale Z-feedrate

Deze waarden zijn nodig zodat andere onderdelen van de pendant, zoals de `JogPlanner`, rekening kunnen houden met de maximale feedrate van de geselecteerde machine.

### Ontwerp

Voeg aan `MachineState` een `Position` toe:

```cpp
Position maxFeedrate;
```

Daarmee wordt de mapping:

```text
maxFeedrate.x → $110
maxFeedrate.y → $111
maxFeedrate.z → $112
```

De waarden zijn feedrates in **mm/min**.

### CNCjs

`CNCjsClientCore` ontvangt de machine-instellingen via het bestaande:

```text
controller:settings
```

event.

De huidige handler doet hier nog niets mee. Deze moet worden uitgebreid zodat de waarden van `$110`, `$111` en `$112` uit de ontvangen controller settings worden gehaald en opgeslagen in:

```text
machineState_.maxFeedrate
```

De bestaande `machineStateSnapshot()` zorgt er vervolgens voor dat deze waarden thread-safe beschikbaar komen voor de rest van de applicatie.

### Belangrijk

De waarden moeten niet uit `Grbl:state` worden afgeleid. Het gaat om **machine-instellingen**, niet om de actuele feedrate.

De implementatie moet aansluiten op de daadwerkelijke JSON-structuur van het `controller:settings` event van CNCjs 1.11.2. Dit kan momenteel nog niet live worden getest omdat de verbinding met de emulator niet beschikbaar is.

### Acceptatiecriteria

* [ ] `MachineState` bevat `maxFeedrate.x`, `.y` en `.z`.
* [ ] `$110` wordt opgeslagen als `maxFeedrate.x`.
* [ ] `$111` wordt opgeslagen als `maxFeedrate.y`.
* [ ] `$112` wordt opgeslagen als `maxFeedrate.z`.
* [ ] De waarden blijven beschikbaar via `machineStateSnapshot()`.
* [ ] De bestaande machine-state verwerking blijft ongewijzigd.
* [ ] Na herstel van de verbinding met de emulator wordt de daadwerkelijke `controller:settings` payload gecontroleerd en wordt de implementatie getest.
