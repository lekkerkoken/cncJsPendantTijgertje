#ifndef JOG_COMMAND_H
#define JOG_COMMAND_H

#include "MachineState.h"
#include "PendantState.h"


enum JogCommandType
{
JOG_NONE,

/*
    Eén onafhankelijke beweging voor één tijdslot.
*/
JOG_MOVE,

/*
    Realtime cancel van een daadwerkelijk lopende GRBL jog.

    De nieuwe planner heeft dit normaal gesproken niet nodig
    voor richtingswisselingen: die worden in de intentiering
    verwerkt.

    Het blijft beschikbaar voor expliciete externe annulering.
*/
JOG_CANCEL

};

struct JogCommand
{
JogCommandType type =
JOG_NONE;

/*
    As waarop deze tijdslot-intentie betrekking heeft.
*/
Axis axis =
    AXIS_NONE;


/*
    Relatieve beweging voor dit tijdslot.

    Dit is géén targetpositie.
*/
float delta =
    0.0f;


/*
    Tijdsduur waarop deze intentie gebaseerd is.

    Voor de huidige planner is dit SLOT_TIME = 50 ms.
*/
unsigned long duration =
    0;


/*
    Feedrate in mm/min waarmee GRBL deze beweging uitvoert.
*/
int feedrate =
    0;

};

#endif
