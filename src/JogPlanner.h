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
        Encoderinput verandert uitsluitend de
        gebruikersintentie/horizon.
    */
    void encoder(const Event& event);

    /*
        Planner wordt vanuit loop() aangeroepen.

        Geeft direct het volgende JogCommand terug.
        Als er niets hoeft te gebeuren is type JOG_NONE.
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


    // ========================================================
    // ENCODER
    // ========================================================

    /*
        Tijdelijke stapgrootte.

        Dit correspondeert momenteel met STEP_0_1_MM.
        Later halen we dit rechtstreeks uit PendantState.
    */
    static constexpr float STEP_SIZE = 0.1f;


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
        Er loopt momenteel een jog.

        Dit is GEEN voorspelling van de machinepositie.
        MachineState blijft de werkelijkheid.
    */
    bool jogActive = false;


    /*
        Richting van de laatst gestuurde jog:

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

    JogCommand requestCancel();

    int calculateFeedrate(
        float remainingDistance
    ) const;

    int direction(
        float distance
    ) const;
};


#endif