#include "JogPlanner.h"

#include <Arduino.h>
#include <math.h>


// ============================================================
// BEGIN
// ============================================================

void JogPlanner::begin()
{
    horizon =
        0.0f;

    positionKnown =
        false;

    intentChanged =
        false;

    reversePulseCount =
        0;

    lastUpdate =
        millis();

    jogActive =
        false;

    plannedTarget =
        0.0f;

    activeDirection =
        0;
}


// ============================================================
// ENCODER
// ============================================================

void JogPlanner::encoder(
    const Event& event
)
{
    if(
        event.type !=
        EVENT_ENCODER_PULSE
    )
    {
        return;
    }


    if(event.value == 0)
    {
        return;
    }


    float oldHorizon =
        horizon;


    /*
        De horizon blijft altijd de gebruikersintentie.

        Iedere encoderpulse verandert de horizon met STEP_SIZE.
    */

    horizon +=
        STEP_SIZE *
        event.value;


    intentChanged =
        true;


    /*
        Wanneer er een actieve jog is en de encoder de andere
        kant op wordt gedraaid, onthouden we iedere pulse.

        Deze pulsen worden later bij de cancel gebruikt om de
        resterende afstand van de actieve jog telkens te
        halveren.
    */

    if(
        jogActive &&
        activeDirection != 0
    )
    {
        int encoderDirection =
            event.value > 0
                ? +1
                : -1;


        if(
            encoderDirection !=
            activeDirection
        )
        {
            reversePulseCount +=
                abs(event.value);
        }
        else
        {
            /*
                Zodra de gebruiker weer dezelfde kant op draait,
                is er geen nieuwe tegengestelde intentie meer.

                De eerder opgebouwde remactie wordt daarom
                opnieuw vanaf nul bekeken.
            */

            reversePulseCount =
                0;
        }
    }


    Serial.print(
        "[JogPlanner] Horizon: "
    );

    Serial.print(
        oldHorizon,
        3
    );

    Serial.print(
        " -> "
    );

    Serial.println(
        horizon,
        3
    );


    if(reversePulseCount > 0)
    {
        Serial.print(
            "[JogPlanner] Reverse pulses: "
        );

        Serial.println(
            reversePulseCount
        );
    }
}


// ============================================================
// UPDATE
// ============================================================

JogCommand JogPlanner::update(
    const MachineState& machineState
)
{
    JogCommand noCommand;


    /*
        Planner draait op vaste frequentie.
    */

    unsigned long now =
        millis();

    if(
        now - lastUpdate <
        PLANNER_INTERVAL
    )
    {
        return noCommand;
    }

    lastUpdate =
        now;


    /*
        MachineState is altijd de werkelijkheid.
    */

    float machinePosition =
        machineState.workPosition.x;


    /*
        ========================================================
        EERSTE GELDIGE POSITIE
        ========================================================
    */

    if(!positionKnown)
    {
        horizon =
            machinePosition;

        positionKnown =
            true;

        intentChanged =
            false;

        reversePulseCount =
            0;

        jogActive =
            false;

        plannedTarget =
            machinePosition;

        activeDirection =
            0;


        Serial.print(
            "[JogPlanner] Initial position: "
        );

        Serial.println(
            machinePosition,
            3
        );


        return noCommand;
    }


    /*
        ========================================================
        ACTIEVE JOG
        ========================================================
    */

    if(jogActive)
    {
        /*
            Een tegengestelde encoderbeweging betekent dat de
            huidige jog niet meer de juiste beweging is.

            We annuleren de huidige $J precies één keer en
            berekenen daarna een nieuwe beweging op basis van
            de resterende afstand.
        */

        if(
            reversePulseCount > 0
        )
        {
            /*
                Bepaal hoeveel van de huidige geplande jog nog
                resteert vanaf de werkelijke machinepositie.
            */

            float remainingJog =
                fabs(
                    plannedTarget -
                    machinePosition
                );


            /*
                Iedere tegengestelde encoderpulse halveert de
                resterende jog.

                Voorbeeld:

                    1.0
                    0.5
                    0.25
                    0.125
                    ...
            */

            for(
                int i = 0;
                i < reversePulseCount;
                ++i
            )
            {
                remainingJog *=
                    0.5f;
            }


            /*
                De cancel is altijd noodzakelijk.

                We mogen de bestaande $J niet laten doorlopen
                terwijl we een nieuwe, kortere beweging plannen.
            */

            JogCommand cancel =
                requestCancel();


            /*
                De remactie is nu verwerkt.
            */

            reversePulseCount =
                0;


            /*
                Als de resterende jog nog minstens STEP_SIZE is,
                plannen we een nieuwe jog over de gehalveerde
                afstand in de oorspronkelijke bewegingsrichting.

                De horizon blijft ondertussen volledig intact.
            */

            if(
                remainingJog >=
                STEP_SIZE
            )
            {
                /*
                    De huidige jog werd geannuleerd.
                    De volgende update moet de nieuwe jog plannen.

                    We slaan de berekende afstand tijdelijk op
                    via plannedTarget en laten jogActive false.

                    Omdat de normale requestMove() de afstand
                    zelf berekent vanuit de horizon, gebruiken we
                    hier de speciale overload.
                */

                float newRemaining =
                    remainingJog;


                /*
                    De nieuwe beweging wordt in dezelfde richting
                    als de oorspronkelijke jog gepland.

                    Dat is bewust: terugdraaien remt eerst de
                    bestaande beweging af. Pas wanneer de horizon
                    daadwerkelijk aan de andere kant van de
                    machinepositie komt, verandert de normale
                    planner van richting.
                */

                JogCommand move =
                    requestMove(
                        machineState,
                        newRemaining,
                        activeDirection
                    );


                if(
                    move.type ==
                    JOG_MOVE
                )
                {
                    intentChanged =
                        false;

                    return move;
                }


                return cancel;
            }


            /*
                De resterende jog is kleiner geworden dan
                STEP_SIZE.

                Dan willen we niet nog een steeds kleinere jog
                uitvoeren.

                De huidige beweging wordt gewoon gecanceld en
                daarmee stoppen we op de actuele machinepositie.

                Daarna leggen we de horizon één STEP_SIZE in de
                nieuwe encoder-richting.

                Vanaf dat moment werkt de normale planner weer.
            */

            int newDirection =
                0;


            if(
                reversePulseCount == 0 &&
                activeDirection != 0
            )
            {
                /*
                    De richting van de laatste tegengestelde
                    encoderbeweging kunnen we hier niet meer uit
                    reversePulseCount halen.

                    De nieuwe richting wordt daarom afgeleid
                    uit de relatie tussen horizon en machine.
                */

                newDirection =
                    direction(
                        horizon -
                        machinePosition
                    );
            }


            /*
                Als de horizon door afronding nog niet aan de
                andere kant van de machinepositie ligt, gebruiken
                we de tegengestelde richting van de oude jog.
            */

            if(newDirection == 0)
            {
                newDirection =
                    -activeDirection;
            }


            horizon =
                machinePosition +
                (
                    newDirection *
                    STEP_SIZE
                );


            intentChanged =
                true;


            /*
                De cancel wordt éénmalig teruggegeven.

                De volgende planner-update ziet:
                
                    jogActive = false
                    horizon   = machine + STEP_SIZE

                en gebruikt vervolgens de normale plannerlogica.
            */

            return cancel;
        }


        /*
            Geen tegengestelde beweging.

            Bepaal de normale gewenste richting.
        */

        int desiredDirection =
            direction(
                horizon -
                machinePosition
            );


        /*
            De horizon is bereikt.
        */

        if(
            fabs(
                horizon -
                machinePosition
            ) <=
            POSITION_EPSILON
        )
        {
            jogActive =
                false;

            plannedTarget =
                machinePosition;

            activeDirection =
                0;

            intentChanged =
                false;

            return noCommand;
        }


        /*
            Is de huidige geplande stap bereikt?

            Dan mag een volgende normale stap worden gepland.
        */

        if(
            targetReached(
                machinePosition
            )
        )
        {
            jogActive =
                false;

            plannedTarget =
                machinePosition;

            activeDirection =
                0;
        }
        else
        {
            /*
                De huidige jog is nog onderweg.

                Een wijziging in dezelfde richting verandert
                alleen de horizon. De huidige $J blijft geldig.
            */

            return noCommand;
        }
    }


    /*
        ========================================================
        GEEN ACTIEVE JOG
        ========================================================
    */

    float remaining =
        horizon -
        machinePosition;


    int desiredDirection =
        direction(
            remaining
        );


    /*
        Horizon bereikt.
    */

    if(
        desiredDirection == 0
    )
    {
        intentChanged =
            false;

        return noCommand;
    }


    /*
        Normale plannerlogica.
    */

    JogCommand command =
        requestMove(
            machineState
        );


    if(
        command.type ==
        JOG_MOVE
    )
    {
        intentChanged =
            false;
    }


    return command;
}


// ============================================================
// REQUEST MOVE
// ============================================================

JogCommand JogPlanner::requestMove(
    const MachineState& machineState
)
{
    float machinePosition =
        machineState.workPosition.x;


    float remaining =
        horizon -
        machinePosition;


    int moveDirection =
        direction(
            remaining
        );


    if(moveDirection == 0)
    {
        JogCommand noCommand;

        return noCommand;
    }


    float distance =
        fabs(
            remaining
        );


    float moveDistance =
        min(
            distance,
            JOG_DISTANCE
        );


    return requestMove(
        machineState,
        moveDistance,
        moveDirection
    );
}


// ============================================================
// REQUEST MOVE WITH DISTANCE
// ============================================================

JogCommand JogPlanner::requestMove(
    const MachineState& machineState,
    float moveDistance,
    int moveDirection
)
{
    JogCommand noCommand;


    if(
        moveDistance <=
        POSITION_EPSILON
    )
    {
        return noCommand;
    }


    if(moveDirection == 0)
    {
        return noCommand;
    }


    float machinePosition =
        machineState.workPosition.x;


    /*
        Een speciale rem-jog mag nooit voorbij de actuele
        horizon worden gepland.

        Dit is alleen relevant wanneer de nieuwe intentie
        inmiddels dichter bij de machinepositie ligt.
    */

    float remainingToHorizon =
        fabs(
            horizon -
            machinePosition
        );


    if(
        remainingToHorizon <=
        POSITION_EPSILON
    )
    {
        return noCommand;
    }


    /*
        Bij een remjog gebruiken we de berekende halve afstand,
        maar nooit meer dan de resterende afstand naar de horizon
        wanneer die in dezelfde richting ligt.
    */

    if(
        direction(
            horizon -
            machinePosition
        ) ==
        moveDirection
    )
    {
        moveDistance =
            min(
                moveDistance,
                remainingToHorizon
            );
    }


    if(
        moveDistance <=
        POSITION_EPSILON
    )
    {
        return noCommand;
    }


    /*
        Feedrate blijft volledig automatisch gekoppeld aan de
        resterende afstand naar de horizon.

        We hoeven dus geen aparte snelheidsregeling te maken.
    */

    int feedrate =
        calculateFeedrate(
            remainingToHorizon
        );


    float target =
        machinePosition +
        (
            moveDirection *
            moveDistance
        );


    JogCommand command;

    command.type =
        JOG_MOVE;

    command.axis =
        AXIS_X;

    command.targetPosition =
        target;

    command.feedrate =
        feedrate;


    jogActive =
        true;

    plannedTarget =
        target;

    activeDirection =
        moveDirection;


    Serial.println(
        "[JogPlanner] JOG_MOVE"
    );

    Serial.print(
        "  machine: "
    );

    Serial.println(
        machinePosition,
        3
    );

    Serial.print(
        "  horizon: "
    );

    Serial.println(
        horizon,
        3
    );

    Serial.print(
        "  target: "
    );

    Serial.println(
        target,
        3
    );

    Serial.print(
        "  distance: "
    );

    Serial.println(
        moveDistance,
        3
    );

    Serial.print(
        "  remaining: "
    );

    Serial.println(
        remainingToHorizon,
        3
    );

    Serial.print(
        "  feedrate: "
    );

    Serial.println(
        feedrate
    );


    return command;
}


// ============================================================
// REQUEST CANCEL
// ============================================================

JogCommand JogPlanner::requestCancel()
{
    JogCommand command;

    command.type =
        JOG_CANCEL;


    /*
        De huidige jog is niet langer geldig.

        MachineState blijft de werkelijkheid.
    */

    jogActive =
        false;

    plannedTarget =
        0.0f;

    activeDirection =
        0;


    /*
        De nieuwe intentie blijft actief.
    */

    intentChanged =
        true;


    Serial.println(
        "[JogPlanner] JOG_CANCEL"
    );


    return command;
}


// ============================================================
// CALCULATE FEEDRATE
// ============================================================

int JogPlanner::calculateFeedrate(
    float remainingDistance
) const
{
    float distance =
        fabs(
            remainingDistance
        );


    /*
        mm / sec
    */

    float velocity =
        distance /
        ARRIVAL_TIME;


    /*
        mm / min
    */

    float feedrate =
        velocity *
        60.0f;


    if(
        feedrate <
        MIN_FEEDRATE
    )
    {
        feedrate =
            MIN_FEEDRATE;
    }


    if(
        feedrate >
        MAX_FEEDRATE
    )
    {
        feedrate =
            MAX_FEEDRATE;
    }


    return (int)feedrate;
}


// ============================================================
// DIRECTION
// ============================================================

int JogPlanner::direction(
    float distance
) const
{
    if(
        distance >
        POSITION_EPSILON
    )
    {
        return +1;
    }


    if(
        distance <
        -POSITION_EPSILON
    )
    {
        return -1;
    }


    return 0;
}


// ============================================================
// TARGET REACHED
// ============================================================

bool JogPlanner::targetReached(
    float machinePosition
) const
{
    if(!jogActive)
    {
        return true;
    }


    float distance =
        plannedTarget -
        machinePosition;


    if(
        activeDirection > 0 &&
        distance <= POSITION_EPSILON
    )
    {
        return true;
    }


    if(
        activeDirection < 0 &&
        distance >= -POSITION_EPSILON
    )
    {
        return true;
    }


    return false;
}