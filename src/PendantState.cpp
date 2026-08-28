#include "PendantState.h"


// ============================================================
// JOG STEP DISTANCE
// ============================================================

float jogStepDistance(
    JogStep jogStep
)
{
    switch(jogStep)
    {
        case STEP_10_MM:

            return 10.0f;


        case STEP_1_MM:

            return 1.0f;


        case STEP_0_1_MM:

            return 0.1f;


        case STEP_0_01_MM:

            return 0.01f;
    }


    return 0.0f;
}