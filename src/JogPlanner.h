#ifndef JOG_PLANNER_H
#define JOG_PLANNER_H

#include "Event.h"
#include "JogCommand.h"
#include "MachineState.h"
#include "MachineSettings.h"

class JogPlanner
{
public:

    void begin(
        const MachineSettings& machineSettings
);


    /*
        Stel de jogafstand in die voor nieuwe encoderintentie
        wordt gebruikt.

        De waarde is afkomstig uit de pendant/state-laag,
        maar JogPlanner hoeft die state zelf niet te kennen.
    */
    void setJogStepDistance(
        float distance
    );


    /*
        Selecteer de as waarop de encoder jogt.
    */
    void setAxis(
        Axis axis
    );


    /*
        Verwerk encoderintentie.

        Iedere encoderstap wordt als intentie over het volledige
        planvenster verdeeld.
    */
    void encoder(
        const Event& event,
        Axis axis
    );


    /*
        Consumeert maximaal één tijdslot uit de intentiering.

        Het resultaat is één onafhankelijke JogCommand voor
        het betreffende tijdslot.
    */
    JogCommand update(
        const MachineState& machineState
    );


private:

    // ========================================================
    // TIME MODEL
    // ========================================================

    /*
        Intentie blijft maximaal 0.5 seconde relevant.

        Bij 20 Hz betekent dit 10 tijdslots van 50 ms.
    */
    static constexpr unsigned long SLOT_TIME = 50;

    static constexpr int SLOT_COUNT = 8;


    /*
        Wanneer een resterende intentie kleiner wordt dan deze
        waarde, beschouwen we hem als praktisch verdwenen.
    */
    static constexpr float INTENT_EPSILON = 0.001f;


    /*
        Bij een richtingswisseling wordt de bestaande toekomstige
        intentie telkens gehalveerd.

        Zodra de resterende intentie voldoende klein is, kan de
        nieuwe richting de ring vullen.
    */
    static constexpr float REVERSAL_FACTOR = 0.5f;


    // ========================================================
    // FEEDRATE
    // ========================================================

    static constexpr int MIN_FEEDRATE = 100;

    Position maxFeedrate;


    // ========================================================
    // RING BUFFER
    // ========================================================

    /*
        Iedere entry is de gewenste relatieve beweging voor één
        tijdslot van SLOT_TIME milliseconden.

        De array is een logische ring:

            writeIndex
            consumeIndex

        De planner hoeft daardoor geen absolute positie of
        einddoel bij te houden.
    */
    float intent[SLOT_COUNT];


    /*
        Slot dat als volgende door update() wordt geconsumeerd.
    */
    int consumeIndex = 0;


    /*
        Geselecteerde encoder-as.
    */
    Axis selectedAxis =
        AXIS_X;


    /*
        Jogafstand voor nieuwe encoderintentie.

        Deze waarde is afkomstig uit de pendant/state-laag.
        JogPlanner kent bewust geen JogStep-enum.
    */
    float jogStepDistance =
        1.0f;


    /*
        Geldige machinepositie ontvangen?

        Dit voorkomt dat direct na startup encoderintentie wordt
        toegevoegd zonder dat we weten of de machine beschikbaar is.
    */
    bool positionKnown = false;


    /*
        Tijdstip waarop het huidige tijdslot beschikbaar komt.
    */
    unsigned long lastUpdate = 0;


    // ========================================================
    // INTERNAL
    // ========================================================

    int nextIndex(
        int index
    ) const;


    void clearIntent();


    void addIntent(
        float delta
    );


    void reverseIntent(
        int direction
    );


    bool hasIntent() const;


    float consumeIntent();


    int calculateFeedrate(
        float delta
    ) const;

    float maxDelta() const;

    float machinePosition(
        const MachineState& machineState,
        Axis axis
    ) const;

};

#endif