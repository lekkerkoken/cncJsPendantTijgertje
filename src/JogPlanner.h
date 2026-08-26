#ifndef JOG_PLANNER_H
#define JOG_PLANNER_H

#include "Event.h"
#include "JogCommand.h"
#include "MachineState.h"


class JogPlanner
{
public:

    void begin();

    /*
        Encoderinput verandert de gebruikersintentie/horizon.

        Een richtingsverandering tijdens een actieve jog wordt
        door de planner gebruikt om de huidige jog af te bouwen.
    */
    void encoder(
        const Event& event
    );

    /*
        Planner wordt vanuit loop() aangeroepen.

        Geeft maximaal één JogCommand terug.
    */
    JogCommand update(
        const MachineState& machineState
    );


private:

    // ========================================================
    // USER INTENT
    // ========================================================

    /*
        De positie waar de gebruiker uiteindelijk naartoe wil.
    */
    float horizon = 0.0f;

    /*
        Geldige machinepositie ontvangen?
    */
    bool positionKnown = false;

    /*
        De gebruikersintentie is gewijzigd sinds de laatste
        geplande beweging.
    */
    bool intentChanged = false;


    // ========================================================
    // ENCODER
    // ========================================================

    /*
        Tijdelijke stapgrootte.

        Dit correspondeert momenteel met STEP_0_1_MM.
        Later halen we dit rechtstreeks uit PendantState.
    */
    static constexpr float STEP_SIZE = 0.1f;


    /*
        Aantal encoderpulsen in de tegengestelde richting
        sinds de actieve jog nog geldig was.

        Iedere pulse halveert bij een cancel de nog resterende
        afstand van de huidige jog.

        Bijvoorbeeld:

            4 pulses
            1.0 mm
            -> 0.5
            -> 0.25
            -> 0.125
            -> 0.0625
    */
    int reversePulseCount = 0;


    // ========================================================
    // PLANNER
    // ========================================================

    /*
        Plannerfrequentie:

            20 Hz
            50 ms
    */
    static constexpr unsigned long PLANNER_INTERVAL = 50;

    unsigned long lastUpdate = 0;


    /*
        Gewenste tijd om de resterende afstand af te leggen.
    */
    static constexpr float ARRIVAL_TIME = 0.8f;


    /*
        Feedrategrenzen.
    */
    static constexpr int MIN_FEEDRATE = 100;
    static constexpr int MAX_FEEDRATE = 3000;


    /*
        Wanneer het verschil tussen horizon en machinepositie
        zo klein is dat geen nieuwe jog nodig is.
    */
    static constexpr float POSITION_EPSILON = 0.01f;


    /*
        Maximale grootte van één normale geplande jogstap.
    */
    static constexpr float JOG_DISTANCE = 1.0f;


    // ========================================================
    // ACTIVE JOG
    // ========================================================

    /*
        Er is momenteel een door de planner uitgegeven jog
        waarvan we nog niet via MachineState hebben vastgesteld
        dat het doel is bereikt.
    */
    bool jogActive = false;


    /*
        Het doel van de momenteel actieve jog.

        Dit is een voorspeld/planningsdoel, geen machinefeedback.
    */
    float plannedTarget = 0.0f;


    /*
        Richting van de actieve jog:

            -1 = negatief
             0 = geen richting
            +1 = positief
    */
    int activeDirection = 0;


    // ========================================================
    // INTERNAL
    // ========================================================

    JogCommand requestMove(
        const MachineState& machineState
    );

    JogCommand requestMove(
        const MachineState& machineState,
        float moveDistance,
        int moveDirection
    );

    JogCommand requestCancel();

    int calculateFeedrate(
        float remainingDistance
    ) const;

    int direction(
        float distance
    ) const;

    bool targetReached(
        float machinePosition
    ) const;
};


#endif