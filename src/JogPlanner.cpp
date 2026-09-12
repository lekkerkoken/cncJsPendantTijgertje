#include "JogPlanner.h"

#include <Arduino.h>
#include <math.h>
#include "MachineSettings.h"


namespace
{
    /*
        Verdeling van één nieuwe encoderintentie over de
        acht toekomstige tijdslots.

            slot 0   15%
            slot 1   15%
            slot 2   15%
            slot 3   15%
            slot 4   15%
            slot 5   13%
            slot 6    9%
            slot 7    3%

        Totaal = 100%.
    */

    constexpr float INTENT_DISTRIBUTION[] =
    {
        0.15f,
        0.15f,
        0.15f,
        0.15f,
        0.15f,
        0.13f,
        0.09f,
        0.03f
    };

    constexpr int INTENT_DISTRIBUTION_COUNT =
        sizeof(INTENT_DISTRIBUTION) /
        sizeof(INTENT_DISTRIBUTION[0]);
}


// ============================================================
// BEGIN
// ============================================================

void JogPlanner::begin(
    const MachineSettings& settings
)
{
    maxFeedrate =
        settings.maxFeedrate;

    clearIntent();

    scaleDownCount = 0;

    consumeIndex =
        0;

    intentDirection =
        0;

    reactionWindowUntil =
        0;

    selectedAxis =
        AXIS_X;

    jogStepDistance =
        1.0f;

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

        Bij een aswissel laten we alle toekomstige
        intentie vervallen.

        De machinepositie blijft echter geldig:
        een aswissel maakt de ontvangen machinepositie
        niet ongeldig.
    */

    selectedAxis =
        axis;

    clearIntent();

    scaleDownCount = 0;

    intentDirection =
        0;

    reactionWindowUntil =
        0;

#ifdef JOGPLANNER_DEBUG

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

#endif
}



// ============================================================
// ENCODER
// ============================================================

void JogPlanner::encoder(
    const Event& event,
    Axis axis
)
{
    Serial.println("[JogPlanner] encoderpulse received");
    if(
        event.type !=
        EVENT_ENCODER_PULSE
    )
    {
        return;
    }

    if(event.value == 0)
    {
        Serial.println("event nul value, this actually never happens");
        return;
    }


    setAxis(axis);


    updateReactionWindow();


    int direction =
        event.value > 0
            ? +1
            : -1;


    int pulses =
        abs(event.value);
    Serial.print("[JogPlanner] pulses: ");
    Serial.println(pulses);
    
        if(intentDirection == 0)
        {
            intentDirection =
                direction;

            addIntent(
                jogStepDistance *
                direction * pulses
            );

            refreshReactionWindow();
        }else if(
            intentDirection ==
            direction
        )
        {

            /*
                Zelfde richting:

                    extra intentie toevoegen.

                    Het JogReactionWindow wordt opnieuw gestart.
            */

            addIntent(
                jogStepDistance *
                direction * pulses
            );

            refreshReactionWindow();

        }else{


        /*
            Tegengestelde richting binnen het actieve
            JogReactionWindow:

                uitsluitend de resterende intentie schalen.

                De intentierichting blijft onveranderd.

                Er wordt GEEN tegengestelde intentie toegevoegd.
        */
        for (int i = 0 ; i<pulses; i++){
        scaleDownIntent();
        }

        refreshReactionWindow();
        }
    


#ifdef JOGPLANNER_DEBUG

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

#endif
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
        Het JogReactionWindow is onafhankelijk van de
        planner-scheduler.

        Het window kan dus ook verlopen terwijl er geen
        JogCommand wordt geproduceerd.
    */

    updateReactionWindow();

    if(scaleDownCount > 3)
    {
        clearIntent();

        scaleDownCount = 0;

        JogCommand command;

        command.type = JOG_CANCEL;

        return command;
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


    if(
        calculatedDeltaAbs >
        maximumDelta
    )
    {
        calculatedDeltaAbs =
            maximumDelta;
    }


    float calculatedDelta =
        deltaIntent > 0.0f
            ? calculatedDeltaAbs
            : -calculatedDeltaAbs;


    command.delta =
        calculatedDelta;


#ifdef JOGPLANNER_DEBUG

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

#endif

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
        De volledige nieuwe encoderintentie wordt over de
        toekomstige tijdslots verdeeld.

        De verdeling is bewust niet uniform:

            slot 0   15%
            slot 1   15%
            slot 2   15%
            slot 3   15%
            slot 4   15%
            slot 5   13%
            slot 6    9%
            slot 7    3%

        Totaal = 100%.

        consumeIndex is het eerstvolgende slot dat door
        update() wordt geconsumeerd.
    */

    static_assert(
        INTENT_DISTRIBUTION_COUNT == SLOT_COUNT,
        "INTENT_DISTRIBUTION must match SLOT_COUNT"
    );


    for(int i = 0; i < SLOT_COUNT; ++i)
    {
        int index =
            (
                consumeIndex +
                i
            )
            %
            SLOT_COUNT;


        intent[index] +=
            delta *
            INTENT_DISTRIBUTION[i];
    }
}



// ============================================================
// SCALE DOWN INTENT
// ============================================================

void JogPlanner::scaleDownIntent()
{
    scaleDownCount++;
}



// ============================================================
// UPDATE REACTION WINDOW
// ============================================================

void JogPlanner::updateReactionWindow()
{
    if(intentDirection == 0)
    {
        return;
    }


    unsigned long now =
        millis();


    /*
        Een window is verlopen zodra de eindtijd is bereikt.

        millis() rollover wordt veilig afgehandeld door de
        gebruikelijke unsigned vergelijking.
    */

    if(
        (long)(
            now -
            reactionWindowUntil
        )
        >=
        0
    )
    {
        /*
            De intentiecontext is nu verlopen.

            Wat al naar de machine is gestuurd kan uiteraard
            niet meer worden teruggedraaid.

            Alleen nog niet geconsumeerde toekomstige intentie
            wordt verwijderd.
        */

        clearIntent();

        intentDirection =
            0;

        reactionWindowUntil =
            0;

        scaleDownCount =
            0;


#ifdef JOGPLANNER_DEBUG

        Serial.println(
            "[JogPlanner] JogReactionWindow expired"
        );

#endif
    }
}



// ============================================================
// REFRESH REACTION WINDOW
// ============================================================

void JogPlanner::refreshReactionWindow()
{
    reactionWindowUntil =
        millis() +
        JOG_REACTION_WINDOW;


#ifdef JOGPLANNER_DEBUG

    Serial.print(
        "[JogPlanner] JogReactionWindow refreshed: "
    );

    Serial.print(
        JOG_REACTION_WINDOW
    );

    Serial.println(
        " ms"
    );

#endif
}



// ============================================================
// CONSUME INTENT
// ============================================================

float JogPlanner::consumeIntent()
{
    float delta =
        intent[consumeIndex];


    intent[consumeIndex] =
        0.0f;


    consumeIndex =
        nextIndex(
            consumeIndex
        );


    float scale =
        powf(
            SCALE_DOWN_FACTOR,
            scaleDownCount
        );


    return delta * scale;
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
            maximum =
                maxFeedrate.x;
            break;

        case AXIS_Y:
            maximum =
                maxFeedrate.y;
            break;

        case AXIS_Z:
            maximum =
                maxFeedrate.z;
            break;

        default:
            maximum =
                0.0f;
            break;
    }


    if(
        feedrate >
        maximum
    )
    {
        feedrate =
            (int)maximum;


#ifdef JOGPLANNER_DEBUG

        Serial.println(
            "[JogPlanner] Max feedrate used"
        );

#endif
    }


    return (int)feedrate;
}



// ============================================================
// MAX DELTA
// ============================================================

float JogPlanner::maxDelta() const
{
    float maximum;


    switch(selectedAxis)
    {
        case AXIS_X:
            maximum =
                maxFeedrate.x;
            break;

        case AXIS_Y:
            maximum =
                maxFeedrate.y;
            break;

        case AXIS_Z:
            maximum =
                maxFeedrate.z;
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
