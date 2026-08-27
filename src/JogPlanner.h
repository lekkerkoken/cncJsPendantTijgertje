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
        Selecteer de as waarop de encoder jogt.

        De planner is niet afhankelijk van PendantState;
        main.cpp geeft alleen de geselecteerde Axis door.
    */
    void setAxis(
        Axis axis
    );


    /*
        Encoderinput verandert de gebruikersintentie/horizon.

        Een richtingsverandering tijdens een actieve jog wordt
        door de planner gebruikt om de huidige jog af te bouwen.
    */
    void encoder(
        const Event& event,
        Axis axis
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
        De positie waar de gebruiker uiteindelijk naartoe wil,
        voor de momenteel geselecteerde as.
    */
    float horizon = 0.0f;


    /*
        Geldige machinepositie ontvangen?
    */
    bool positionKnown = false;


    /*
        De momenteel geselecteerde jog-as.
    */
    Axis selectedAxis =
        AXIS_X;


    /*
        De as waarop de momenteel actieve jog daadwerkelijk
        draait.

        Dit is belangrijk wanneer de gebruiker tijdens een
        actieve jog van as wisselt.
    */
    Axis activeAxis =
        AXIS_NONE;


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
    */
    int reversePulseCount = 0;


    /*
        Richting van de laatste tegengestelde encoderbeweging.

        Hiermee kunnen we ook bij zeer kleine resterende
        afstanden correct bepalen wat de nieuwe intentie is.
    */
    int reverseDirection = 0;


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
    static constexpr float ARRIVAL_TIME = 0.4f;


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

    float machinePosition(
        const MachineState& machineState,
        Axis axis
    ) const;


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