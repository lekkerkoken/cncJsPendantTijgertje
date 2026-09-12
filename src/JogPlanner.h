#ifndef JOG_PLANNER_H
#define JOG_PLANNER_H

#include "Event.h"
#include "JogCommand.h"
#include "MachineState.h"
#include "MachineSettings.h"
#include "Config.h"

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

        Iedere encoderpulse zet of beïnvloedt een
        intentierichting.

        De pulse wordt direct vertaald naar de intentiering.
        JogReactionWindow bepaalt alleen hoe een volgende
        pulse geïnterpreteerd wordt.
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
        Iedere ringbuffer-entry vertegenwoordigt één tijdslot.
    */
    static constexpr unsigned long SLOT_TIME = 50;

    static constexpr int SLOT_COUNT = 8;


    /*
        Het JogReactionWindow duurt even lang als het
        planvenster.

        Een encoderpulse start het window of verlegt het
        window opnieuw.
    */
    static constexpr unsigned long JOG_REACTION_WINDOW =
        SLOT_TIME * SLOT_COUNT+300;


    /*
        Wanneer een resterende intentie kleiner wordt dan deze
        waarde, beschouwen we hem als praktisch verdwenen.
    */
    static constexpr float INTENT_EPSILON = 0.001f;


    /*
        Iedere tegengestelde pulse binnen het actieve
        JogReactionWindow schaalt de resterende intentie.

        De intentierichting zelf verandert daarbij niet.
    */
    static constexpr float SCALE_DOWN_FACTOR = 0.68f;


    // ========================================================
    // FEEDRATE
    // ========================================================

    static constexpr int MIN_FEEDRATE = 10;

    Position maxFeedrate;


    // ========================================================
    // RING BUFFER
    // ========================================================

    /*
        Iedere entry is de gewenste relatieve beweging voor één
        tijdslot van SLOT_TIME milliseconden.

        De array is een logische ring.

        De planner hoeft daardoor geen absolute positie of
        einddoel bij te houden.
    */
    float intent[SLOT_COUNT];


    /*
        Slot dat als volgende door update() wordt geconsumeerd.
    */
    int consumeIndex = 0;


    // ========================================================
    // INTENT DIRECTION
    // ========================================================

    /*
        Actieve intentierichting:

            -1 = negatieve richting
             0 = geen actieve intentie
            +1 = positieve richting

        De richting blijft actief zolang het
        JogReactionWindow niet verlopen is.
    */
    int intentDirection = 0;


    /*
        Tijdstip waarop het huidige JogReactionWindow verloopt.
    */
    unsigned long reactionWindowUntil = 0;


    // ========================================================
    // JOG STATE
    // ========================================================

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


    /*
        Verminder uitsluitend de nog niet geconsumeerde intentie.

        Dit zet geen nieuwe tegengestelde intentie.
    */
    void scaleDownIntent();


    /*
        Controleer of het JogReactionWindow nog actief is.

        Wanneer het window verlopen is, wordt de actieve
        intentierichting unset en wordt resterende toekomstige
        intentie verwijderd.
    */
    void updateReactionWindow();


    /*
        Start of verleg het JogReactionWindow.
    */
    void refreshReactionWindow();


    int scaleDownCount = 0;


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