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
