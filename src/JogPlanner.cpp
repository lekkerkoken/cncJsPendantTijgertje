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

    selectedAxis =
        AXIS_X;

    activeAxis =
        AXIS_NONE;

    intentChanged =
        false;

    reversePulseCount =
        0;

    reverseDirection =
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
// SET AXIS
// ============================================================

void JogPlanner::setAxis(
    Axis axis
)
{
    if(axis == AXIS_NONE)
    {
        return;
    }


    /*
        Wanneer de as verandert, moet de horizon opnieuw
        worden gesynchroniseerd met de actuele positie van
        de nieuwe as.

        Een actieve jog op de oude as mag niet doorlopen
        als nieuwe as-intentie.
    */

    if(axis != selectedAxis)
    {
        selectedAxis =
            axis;

        positionKnown =
            false;

        jogActive =
            false;

        activeAxis =
            AXIS_NONE;

        plannedTarget =
            0.0f;

        activeDirection =
            0;

        reversePulseCount =
            0;

        reverseDirection =
            0;

        intentChanged =
            false;
    }
}


// ============================================================
// ENCODER
// ============================================================

void JogPlanner::encoder(
    const Event& event,
    Axis axis
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


    /*
        De encoder hoort bij de op dit moment geselecteerde as.
    */

    setAxis(axis);


    /*
        ========================================================
        POSITIE NOG NIET GESYNCHRONISEERD
        ========================================================

        Er is nog geen geldige horizon voor deze as.

        Zonder actuele machinepositie kunnen we de encoderstap
        niet veilig aan een absolute horizon koppelen.

        Daarom negeren we de puls hier.
    */

    if(!positionKnown)
    {
        Serial.println(
            "[JogPlanner] Encoder ignored: position not known"
        );

        return;
    }


    float oldHorizon =
        horizon;


    /*
        ========================================================
        POSITIE IS WEL BEKEND
        ========================================================

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

        Deze pulsen worden gebruikt om de huidige actieve jog
        eerst gecontroleerd af te remmen.
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

            reverseDirection =
                encoderDirection;
        }
        else
        {
            reversePulseCount =
                0;

            reverseDirection =
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
        ========================================================
        EERSTE MACHINEPOSITIE
        ========================================================

        Bij startup is horizon nog geen geldige gebruikersintentie.

        We synchroniseren de horizon daarom éénmalig met de
        actuele positie van de geselecteerde as.

        BELANGRIJK:

        Deze update mag NOOIT zelf een jog veroorzaken.

        De volgende update begint vanuit een stabiele toestand:

            horizon == machinePosition
    */

    if(!positionKnown)
    {
        if(!machineState.connected)
        {
            return noCommand;
        }
        horizon =
            machinePosition(
                machineState,
                selectedAxis
            );


        positionKnown =
            true;


        intentChanged =
            false;

        jogActive =
            false;

        activeAxis =
            AXIS_NONE;

        plannedTarget =
            horizon;

        activeDirection =
            0;

        reversePulseCount =
            0;

        reverseDirection =
            0;


        Serial.print(
            "[JogPlanner] Initial position: "
        );

        Serial.println(
            horizon,
            3
        );


        return noCommand;
    }


    /*
        MachineState is vanaf hier de werkelijkheid.
    */

    float currentPosition =
        machinePosition(
            machineState,
            selectedAxis
        );


    /*
        ========================================================
        ACTIEVE JOG
        ========================================================
    */

    if(jogActive)
    {
        /*
            Een tegengestelde encoderbeweging betekent dat de
            huidige jog niet meer volledig overeenkomt met de
            gebruikersintentie.

            Eerst moet de lopende $J worden gecanceld.
        */

        if(
            reversePulseCount > 0
        )
        {
            reversePulseCount =
                0;


            reverseDirection =
                0;


            return requestCancel();
        }


        /*
            Geen tegengestelde beweging.

            Bepaal of de huidige geplande jog zijn doel heeft
            bereikt.
        */

        if(
            targetReached(
                currentPosition
            )
        )
        {
            jogActive =
                false;

            plannedTarget =
                currentPosition;

            activeDirection =
                0;

            activeAxis =
                AXIS_NONE;
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
        currentPosition;


    int desiredDirection =
        direction(
            remaining
        );


    /*
        Horizon bereikt.
    */

    if(desiredDirection == 0)
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
// MACHINE POSITION
// ============================================================

float JogPlanner::machinePosition(
    const MachineState& machineState,
    Axis axis
) const
{
    switch(axis)
    {
        case AXIS_X:

            return machineState.workPosition.x;


        case AXIS_Y:

            return machineState.workPosition.y;


        case AXIS_Z:

            return machineState.workPosition.z;


        case AXIS_NONE:

        default:

            return 0.0f;
    }
}


// ============================================================
// REQUEST MOVE
// ============================================================

JogCommand JogPlanner::requestMove(
    const MachineState& machineState
)
{
    float currentPosition =
        machinePosition(
            machineState,
            selectedAxis
        );


    float remaining =
        horizon -
        currentPosition;


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


    float currentPosition =
        machinePosition(
            machineState,
            selectedAxis
        );


    float remainingToHorizon =
        fabs(
            horizon -
            currentPosition
        );


    if(
        remainingToHorizon <=
        POSITION_EPSILON
    )
    {
        return noCommand;
    }


    /*
        Wanneer de beweging dezelfde richting heeft als de
        horizon, mogen we nooit voorbij de horizon bewegen.
    */

    if(
        direction(
            horizon -
            currentPosition
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
        Feedrate blijft automatisch gekoppeld aan de resterende
        afstand naar de horizon.
    */

    int feedrate =
        calculateFeedrate(
            remainingToHorizon
        );


    float target =
        currentPosition +
        (
            moveDirection *
            moveDistance
        );


    JogCommand command;

    command.type =
        JOG_MOVE;

    command.axis =
        selectedAxis;

    command.targetPosition =
        target;

    command.feedrate =
        feedrate;


    jogActive =
        true;

    activeAxis =
        selectedAxis;

    plannedTarget =
        target;

    activeDirection =
        moveDirection;


    Serial.println(
        "[JogPlanner] JOG_MOVE"
    );

    Serial.print(
        "  axis: "
    );

    switch(selectedAxis)
    {
        case AXIS_X:
            Serial.println("X");
            break;

        case AXIS_Y:
            Serial.println("Y");
            break;

        case AXIS_Z:
            Serial.println("Z");
            break;

        default:
            Serial.println("NONE");
            break;
    }

    Serial.print(
        "  machine: "
    );

    Serial.println(
        currentPosition,
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

    activeAxis =
        AXIS_NONE;


    /*
        De nieuwe intentie blijft volledig behouden in horizon.
    */

    intentChanged =
        true;

    positionKnown = false;
    
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
    float currentPosition
) const
{
    if(!jogActive)
    {
        return true;
    }


    float distance =
        plannedTarget -
        currentPosition;


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