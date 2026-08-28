#ifndef PENDANT_STATE_H
#define PENDANT_STATE_H


enum PendantLayer
{
    LAYER_JOG,
    LAYER_INFO,
    LAYER_CONTROL
};


enum Axis
{
    AXIS_NONE,
    AXIS_X,
    AXIS_Y,
    AXIS_Z
};


enum JogStep
{
    STEP_10_MM,
    STEP_1_MM,
    STEP_0_1_MM,
    STEP_0_01_MM
};


/*
    Centrale vertaling van de geselecteerde JogStep
    naar de daadwerkelijke jogafstand in millimeters.

    JogStep is de state.
    Deze functie is de enige vertaling naar een fysieke waarde.
*/
float jogStepDistance(
    JogStep jogStep
);


struct PendantState
{
    PendantLayer layer =
        LAYER_JOG;


    Axis axis =
        AXIS_X;


    JogStep jogStep =
        STEP_1_MM;


    bool controlLocked =
        true;
};


#endif