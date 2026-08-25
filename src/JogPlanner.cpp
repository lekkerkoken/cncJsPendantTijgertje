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
    /*
        Encoderinput verandert alleen de gebruikershorizon.

        Er wordt hier dus GEEN machinecommando gegenereerd.

        De encoder markeert wel dat de gebruikersintentie
        gewijzigd is. De planner gebruikt dit om een eventueel
        actieve planning opnieuw te beoordelen.
    */

    if(
        event.type !=
        EVENT_ENCODER_PULSE
    )
    {
        return;
    }


    /*
        Geen wijziging betekent ook geen gewijzigde intentie.
    */

    if(event.value == 0)
    {
        return;
    }


    float oldHorizon =
        horizon;


    horizon +=
        STEP_SIZE *
        event.value;


    /*
        Iedere daadwerkelijke encoderbeweging verandert de
        gebruikersintentie.

        Dit betekent niet automatisch dat er een nieuw
        CNCjs-commando moet worden gestuurd.
    */

    intentChanged =
        true;


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
}


// ============================================================
// UPDATE
// ============================================================

JogCommand JogPlanner::update(
    const MachineState& machineState
)
{
    /*
        Standaard: er hoeft niets te gebeuren.
    */

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
        We hebben actuele machinefeedback nodig.

        MachineState is de werkelijkheid.
        Horizon is uitsluitend gebruikersintentie.
    */

    float machinePosition =
        machineState.workPosition.x;


    /*
        Eerste geldige positie.

        De horizon wordt éénmalig aan de werkelijke machinepositie
        gekoppeld.
    */

    if(!positionKnown)
    {
        horizon =
            machinePosition;

        positionKnown =
            true;

        intentChanged =
            false;

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

        Eerst beoordelen we of de gebruiker tijdens een lopende
        beweging zijn intentie heeft gewijzigd.
    */

    if(jogActive)
    {
        int desiredDirection =
            direction(
                horizon -
                machinePosition
            );


        /*
            De gebruiker heeft de intentie gewijzigd.

            Als de nieuwe gewenste richting tegengesteld is aan
            de actieve jog, moet de huidige jog eerst worden
            geannuleerd.

            Dit voorkomt dat de oude beweging doorloopt terwijl
            de gebruiker inmiddels de andere kant op draait.
        */

        if(
            intentChanged &&
            desiredDirection != 0 &&
            activeDirection != 0 &&
            desiredDirection != activeDirection
        )
        {
            return requestCancel();
        }


        /*
            De horizon is bereikt.

            Ook wanneer de gebruiker tijdens de beweging nog
            encoderpulsen in dezelfde richting heeft gegeven,
            kan de huidige stap gewoon worden afgemaakt.
        */

        if(
            fabs(
                horizon -
                machinePosition
            ) <=
            POSITION_EPSILON
        )
        {
            /*
                De actieve jog is fysiek voltooid.

                Er hoeft niets meer te worden gestuurd.
            */

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
            Is de huidige geplande stap al bereikt?

            Dan mogen we een volgende stap plannen.
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

            /*
                De intentie mag hier blijven staan.

                Als de horizon verder weg ligt, zal hieronder
                onmiddellijk een volgende stap worden gepland.
            */
        }
        else
        {
            /*
                De huidige jog is nog onderweg.

                Zonder een relevante intentiewijziging doen we
                absoluut niets.

                Dit is het belangrijke verschil met de vorige
                implementatie: dezelfde JOG_MOVE wordt niet iedere
                50 ms opnieuw uitgegeven.
            */

            if(!intentChanged)
            {
                return noCommand;
            }


            /*
                De gebruiker heeft de horizon gewijzigd, maar
                niet van richting.

                De lopende jog hoeft daarom niet opnieuw te
                worden uitgegeven. We wachten totdat de huidige
                stap is bereikt.
            */

            return noCommand;
        }
    }


    /*
        ========================================================
        GEEN ACTIEVE JOG
        ========================================================

        Nu bepalen we op basis van de actuele machinepositie
        en de horizon of er een nieuwe stap nodig is.
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
        Er is geen actieve jog meer.

        Plan nu één nieuwe stap richting de horizon.
    */

    JogCommand command =
        requestMove(
            machineState
        );


    /*
        Een succesvolle nieuwe beweging consumeert de
        intentiewijziging.

        Een eventuele volgende stap kan daarna alleen worden
        gepland wanneer de machine de huidige stap heeft bereikt
        of wanneer de gebruiker opnieuw aan de encoder draait.
    */

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
    JogCommand noCommand;


    float machinePosition =
        machineState.workPosition.x;


    float remaining =
        horizon -
        machinePosition;


    if(
        fabs(remaining) <=
        POSITION_EPSILON
    )
    {
        return noCommand;
    }


    int moveDirection =
        direction(
            remaining
        );


    if(moveDirection == 0)
    {
        return noCommand;
    }


    /*
        Feedrate wordt bepaald op basis van de volledige
        resterende afstand naar de horizon.

        De grootte van de daadwerkelijke jogstap staat daar
        los van.
    */

    int feedrate =
        calculateFeedrate(
            remaining
        );


    /*
        Eén plannerstap is maximaal JOG_DISTANCE.

        De gebruiker kan dus bijvoorbeeld 5 mm aanvragen,
        maar de planner zal dat uitvoeren als:

            1 mm
            1 mm
            1 mm
            1 mm
            1 mm

        waarbij iedere volgende stap pas wordt gepland nadat
        MachineState de vorige stap heeft bevestigd.
    */

    float distance =
        fabs(remaining);


    float moveDistance =
        min(
            distance,
            JOG_DISTANCE
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


    /*
        Vanaf dit moment is er een actieve jog.

        plannedTarget is uitsluitend een planningswaarde.
        De echte positie blijft uit MachineState komen.
    */

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
        "  remaining: "
    );

    Serial.println(
        remaining,
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

        De werkelijke machinepositie wordt na de cancel opnieuw
        via MachineState vastgesteld.

        De horizon blijft uiteraard bestaan: die representeert
        nog steeds de nieuwe gebruikersintentie.
    */

    jogActive =
        false;

    plannedTarget =
        0.0f;

    activeDirection =
        0;


    /*
        De intentie is nog steeds gewijzigd en moet dus na de
        cancel opnieuw worden geëvalueerd.
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


    /*
        We gebruiken geen exacte gelijkheid.

        De machinefeedback kan tussen twee plannerupdates
        over het doel heen zijn gegaan.
    */

    float distance =
        plannedTarget -
        machinePosition;


    /*
        Een doel is bereikt wanneer de machine op of voorbij
        het geplande doel is gekomen in de bewegingsrichting.
    */

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