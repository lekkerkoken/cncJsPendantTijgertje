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

        Een encoderbeweging markeert daarmee dat de
        gebruikersintentie is gewijzigd.
    */
    void encoder(const Event& event);

    /*
        Planner wordt vanuit loop() aangeroepen.

        Geeft maximaal één JogCommand terug.

        De planner geeft niet continu dezelfde jog opnieuw
        uit. Een nieuwe stap wordt pas gepland wanneer de
        vorige stap daadwerkelijk door MachineState is
        bereikt, of wanneer de gebruikersintentie verandert.
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

        Dit wordt door encoder() gezet.
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
        Maximale grootte van één geplande jogstap.

        Dit is nadrukkelijk NIET de maximale afstand die de
        gebruiker kan aanvragen.

        Bij een horizon van bijvoorbeeld +5 mm worden meerdere
        stappen van maximaal 1 mm gepland.
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
        MachineState blijft altijd de werkelijkheid.
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