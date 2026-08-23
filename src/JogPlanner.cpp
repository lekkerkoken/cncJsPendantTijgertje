#include "JogPlanner.h"

#include <Arduino.h>
#include <math.h>


// ============================================================
// BEGIN
// ============================================================

void JogPlanner::begin()
{
    horizon = 0.0f;

    positionKnown = false;

    lastUpdate = millis();

    jogActive = false;

    activeDirection = 0;
}


// ============================================================
// ENCODER
// ============================================================

void JogPlanner::encoder(const Event& event)
{
    /*
        Encoderinput verandert alleen de gebruikershorizon.

        Er wordt hier dus GEEN machinecommando gegenereerd.

        Dit is belangrijk: een encoder die bijvoorbeeld
        75 pulsen/s genereert, veroorzaakt niet automatisch
        75 CNCjs-commando's/s.
    */

    switch(event.type)
    {
        case EVENT_ENCODER_PULSE:

            horizon += STEP_SIZE;

            break;


        default:

            break;
    }

    Serial.print("[JogPlanner] Horizon: ");
    Serial.println(horizon, 3);
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

    unsigned long now = millis();

    if(now - lastUpdate < PLANNER_INTERVAL)
    {
        return noCommand;
    }

    lastUpdate = now;


    /*
        We hebben actuele machinefeedback nodig.

        De MachineState is de werkelijkheid.
        De horizon is slechts gebruikersintentie.
    */

    float machinePosition = 0.0f;


    /*
        Bepaal voorlopig de actieve as.

        Voor de huidige eerste implementatie gebruiken we
        X als standaard.

        Dit wordt later gekoppeld aan PendantState.
    */

    machinePosition =
        machineState.workPosition.x;


    if(!positionKnown)
    {
        /*
            Eerste geldige positie.

            De eerste horizon moet op de werkelijke positie
            worden gesynchroniseerd.

            Anders zou de eerste encoderpuls kunnen vertrekken
            vanaf een willekeurige beginwaarde.
        */

        horizon = machinePosition;

        positionKnown = true;

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
        Bereken de resterende afstand naar de horizon.
    */

    float remaining =
        horizon - machinePosition;


    /*
        Gebruiker heeft zijn horizon gewijzigd naar de
        andere kant van de actieve beweging.

        Dan moeten we de bestaande jog annuleren.

        Dit is precies waar JOG_CANCEL voor dient.
    */

    int desiredDirection =
        direction(remaining);


    if(
        jogActive &&
        desiredDirection != 0 &&
        activeDirection != 0 &&
        desiredDirection != activeDirection
    )
    {
        return requestCancel();
    }


    /*
        Horizon bereikt.
    */

    if(fabs(remaining) <= POSITION_EPSILON)
    {
        if(jogActive)
        {
            return requestCancel();
        }

        return noCommand;
    }


    /*
        Als er al een jog loopt en de richting nog steeds
        klopt, hoeven we niet iedere 50 ms een nieuwe jog
        te sturen.

        We willen juist voorkomen dat CNCjs overspoeld wordt
        met een waterval aan commando's.
    */

    if(jogActive)
    {
        return noCommand;
    }


    /*
        Geen actieve jog:

        stuur één nieuwe korte jog in de richting van de
        gebruikershorizon.
    */

    return requestMove(machineState);
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
        horizon - machinePosition;


    if(fabs(remaining) <= POSITION_EPSILON)
    {
        return noCommand;
    }


    int moveDirection =
        direction(remaining);


    if(moveDirection == 0)
    {
        return noCommand;
    }


    /*
        Bereken feedrate op basis van de volledige resterende
        afstand en de gewenste aankomsttijd.
    */

    int feedrate =
        calculateFeedrate(remaining);


    /*
        We sturen NIET de volledige afstand naar de horizon.

        We sturen een korte jog.

        Daardoor blijft de interne GRBL jogbuffer klein
        en kan 0x85 de beweging snel annuleren wanneer
        de gebruiker van richting verandert.

        De exacte lengte wordt later verder afgestemd op
        machinefeedrate en acceleratie.
    */

    static constexpr float JOG_DISTANCE = 1.0f;


    float distance =
        fabs(remaining);


    float moveDistance =
        min(distance, JOG_DISTANCE);


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
        Vanaf dit moment beschouwen we de jog als actief.

        De werkelijke machinepositie blijft echter uitsluitend
        afkomstig uit MachineState.
    */

    jogActive = true;

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
        We mark the jog as no longer active locally.

        De werkelijke machinepositie wordt daarna opnieuw
        bepaald via MachineState.
    */

    jogActive = false;

    activeDirection = 0;


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
        fabs(remainingDistance);


    /*
        mm / sec
    */

    float velocity =
        distance /
        ARRIVAL_TIME;


    /*
        mm/min
    */

    float feedrate =
        velocity *
        60.0f;


    if(feedrate < MIN_FEEDRATE)
    {
        feedrate =
            MIN_FEEDRATE;
    }


    if(feedrate > MAX_FEEDRATE)
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
    if(distance > POSITION_EPSILON)
    {
        return +1;
    }

    if(distance < -POSITION_EPSILON)
    {
        return -1;
    }

    return 0;
}