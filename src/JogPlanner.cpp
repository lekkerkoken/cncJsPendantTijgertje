#include "JogPlanner.h"

#include <Arduino.h>
#include <math.h>
#include "MachineSettings.h"


// ============================================================
// BEGIN
// ============================================================

void JogPlanner::begin(
    const MachineSettings& settings
)
{
    maxFeedrate = settings.maxFeedrate;

    clearIntent();

    consumeIndex =
        0;

    selectedAxis =
        AXIS_X;

    jogStepDistance =
        1.0f;

    positionKnown =
        false;

    lastUpdate =
        millis();
}



// ============================================================
// SET JOG STEP DISTANCE
// ============================================================

void JogPlanner::setJogStepDistance(
    float distance
)
{
    if(distance <= 0.0f)
    {
        return;
    }


    jogStepDistance =
        distance;
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

    if(axis == selectedAxis)
    {
        return;
    }


    /*
        Intentie hoort altijd bij één specifieke as.

        Bij een aswissel laten we daarom alle toekomstige
        intentie vervallen. De nieuwe as begint schoon.
    */

    selectedAxis =
        axis;

    clearIntent();

    positionKnown =
        false;


    Serial.print(
        "[JogPlanner] Axis changed: "
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


    setAxis(axis);


    /*
        Zonder geldige machineverbinding accepteren we nog geen
        jogintentie.
    */

    if(!positionKnown)
    {
        Serial.println(
            "[JogPlanner] Encoder ignored: position not known"
        );

        return;
    }


    int direction =
        event.value > 0
            ? +1
            : -1;


    int pulses =
        abs(event.value);


    for(int i = 0; i < pulses; ++i)
    {
        /*
            Zelfde richting:

                voeg één nieuwe intentiestap toe.

            Tegengestelde richting:

                breek de bestaande toekomstige intentie af.
        */

        bool sameDirection =
            true;


        for(int slot = 0; slot < SLOT_COUNT; ++slot)
        {
            if(
                fabs(
                    intent[slot]
                ) >
                INTENT_EPSILON
            )
            {
                int existingDirection =
                    intent[slot] > 0
                        ? +1
                        : -1;


                if(
                    existingDirection !=
                    direction
                )
                {
                    sameDirection =
                        false;

                    break;
                }
            }
        }


        if(sameDirection)
        {
            addIntent(
                jogStepDistance *
                direction
            );
        }
        else
        {
            reverseIntent(
                direction
            );
        }
    }


    Serial.print(
        "[JogPlanner] Encoder intent: "
    );


    Serial.print(
        event.value
    );


    Serial.print(
        "  direction="
    );


    Serial.print(
        direction
    );


    Serial.print(
        "  step="
    );


    Serial.println(
        jogStepDistance,
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
    JogCommand noCommand;


    unsigned long now =
        millis();


    /*
        Eerste geldige machinepositie.

        De positie is uitsluitend validatie/kalibratie.
        Hij bepaalt NIET de inhoud van de intentiering.
    */

    if(!positionKnown)
    {
        if(!machineState.machineStatus == MACHINE_IDLE ||
!machineState.machineStatus == MACHINE_RUN)
        {
            return noCommand;
        }


        positionKnown =
            true;

        lastUpdate =
            now;


        Serial.print(
            "[JogPlanner] Position synchronized: "
        );


        Serial.println(
            machinePosition(
                machineState,
                selectedAxis
            ),
            3
        );


        return noCommand;
    }


    /*
        Planner draait op SLOT_TIME.

        Alles wat vóór dit moment voor het volgende tijdslot
        bedoeld was, wordt nu geconsumeerd.
    */

    if(
        now - lastUpdate <
        SLOT_TIME
    )
    {
        return noCommand;
    }


    lastUpdate +=
        SLOT_TIME;


    /*
        Consumeer precies één slot.

        Een leeg/verlopen slot levert bewust géén command op.
    */

    float deltaIntent =
        consumeIntent();


    if(
        fabs(deltaIntent) <=
        INTENT_EPSILON
    )
    {
        return noCommand;
    }


    JogCommand command;

    command.type =
        JOG_MOVE;

    command.axis =
        selectedAxis;

    command.feedrate =
        calculateFeedrate(
            deltaIntent
        );

    command.duration =
        SLOT_TIME;


    float maximumDelta =
        maxDelta();


    float calculatedDeltaAbs =
        fabs(deltaIntent);


    if(calculatedDeltaAbs > maximumDelta)
    {
        calculatedDeltaAbs =
            maximumDelta;
    }

    float calculatedDelta =  deltaIntent > 0.0f
            ? calculatedDeltaAbs
            : -calculatedDeltaAbs;


    command.delta = calculatedDelta;


    Serial.println(
        "[JogPlanner] JOG_MOVE"
    );


    Serial.print(
        "  delta: "
    );


    Serial.println(
        calculatedDelta,
        4
    );


    Serial.print(
        "  duration: "
    );


    Serial.print(
        command.duration
    );


    Serial.println(
        " ms"
    );


    Serial.print(
        "  feedrate: "
    );


    Serial.println(
        command.feedrate
    );


    return command;
}



// ============================================================
// NEXT INDEX
// ============================================================

int JogPlanner::nextIndex(
    int index
) const
{
    ++index;


    if(index >= SLOT_COUNT)
    {
        index =
            0;
    }


    return index;
}



// ============================================================
// CLEAR INTENT
// ============================================================

void JogPlanner::clearIntent()
{
    for(int i = 0; i < SLOT_COUNT; ++i)
    {
        intent[i] =
            0.0f;
    }
}



// ============================================================
// ADD INTENT
// ============================================================

void JogPlanner::addIntent(
    float delta
)
{
    /*
        De volledige nieuwe encoderintentie wordt over alle
        toekomstige tijdslots verdeeld.

            Daardoor blijft:

                INTENT_LIFETIME == PLAN_AHEAD

            en ontstaat geen onbeperkte toekomstbuffer.
    */

    float perSlot =
        delta /
        SLOT_COUNT;


    for(int i = 0; i < SLOT_COUNT; ++i)
    {
        int index =
            (
                consumeIndex +
                i
            ) %
            SLOT_COUNT;


        intent[index] +=
            perSlot;
    }
}



// ============================================================
// REVERSE INTENT
// ============================================================

void JogPlanner::reverseIntent(
    int direction
)
{
    /*
        Een richtingswisseling probeert NIET een bestaand
        commando te annuleren.

            We veranderen de nog niet verlopen intentie.

            Iedere tegengestelde encoderstap halveert de resterende
            oude intentie.

            Daardoor wordt bijvoorbeeld:

                ++++++++

            eerst:

                ++++....

            daarna:

                ++......

            daarna:

                +.......

            waarna de nieuwe richting de ring kan overnemen.
    */

    bool oldIntentRemaining =
        false;


    for(int i = 0; i < SLOT_COUNT; ++i)
    {
        int index =
            (
                consumeIndex +
                i
            ) %
            SLOT_COUNT;


        intent[index] *=
            REVERSAL_FACTOR;


        if(
            fabs(
                intent[index]
            ) >
            INTENT_EPSILON
        )
        {
            oldIntentRemaining =
                true;
        }
    }


    /*
        Zodra de oude intentie praktisch verdwenen is, vullen we
        de toekomst met de nieuwe richting.

        De oude intentie hoeft nooit te worden teruggestuurd.
    */

    if(!oldIntentRemaining)
    {
        addIntent(
            jogStepDistance *
            direction
        );


        Serial.println(
            "[JogPlanner] Direction reversed"
        );
    }
}



// ============================================================
// HAS INTENT
// ============================================================

bool JogPlanner::hasIntent() const
{
    for(int i = 0; i < SLOT_COUNT; ++i)
    {
        if(
            fabs(
                intent[i]
            ) >
            INTENT_EPSILON
        )
        {
            return true;
        }
    }


    return false;
}



// ============================================================
// CONSUME INTENT
// ============================================================

float JogPlanner::consumeIntent()
{
    float delta =
        intent[consumeIndex];


    /*
        Dit slot is vanaf nu verleden tijd.

        Wat hier zat kan nooit meer worden meegestuurd.
    */

    intent[consumeIndex] =
        0.0f;


    consumeIndex =
        nextIndex(
            consumeIndex
        );


    return delta;
}



// ============================================================
// CALCULATE FEEDRATE
// ============================================================

int JogPlanner::calculateFeedrate(
    float delta
) const
{
    float distance =
        fabs(
            delta
        );


    /*
        mm / sec

        De intentie van dit slot moet binnen de tijdsbasis
        van het slot worden uitgevoerd.
    */

    float velocity =
        distance /
        (
            SLOT_TIME /
            1000.0f
        );


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

    float maximum;

    switch(selectedAxis)
    {
        case AXIS_X:
            maximum = maxFeedrate.x;
            break;

        case AXIS_Y:
            maximum = maxFeedrate.y;
            break;

        case AXIS_Z:
            maximum = maxFeedrate.z;
            break;

        default:
            maximum = 0.0f;
            break;
    }

    if(
        feedrate >
        maximum
    )
    {
        feedrate =
            (int)maximum;
            Serial.println(
            "[JogPlanner] Max feedrate used"
        );

    }



    return (int)feedrate;
}

float JogPlanner::maxDelta() const
{
    float maximum;

    switch(selectedAxis)
    {
        case AXIS_X:
            maximum = maxFeedrate.x;
            break;

        case AXIS_Y:
            maximum = maxFeedrate.y;
            break;

        case AXIS_Z:
            maximum = maxFeedrate.z;
            break;

        default:
            return 0.0f;
    }

    return
        maximum /
        60.0f *
        SLOT_TIME /
        1000.0f;
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