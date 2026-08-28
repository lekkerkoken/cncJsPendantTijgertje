#ifndef MACHINE_MAPPER_H
#define MACHINE_MAPPER_H

#include "JogCommand.h"
#include "MachineCommand.h"
#include "MachineState.h"

class MachineMapper
{
public:


void begin();


void update(
    const JogCommand& jog,
    const MachineState& machineState
);


bool available();


MachineCommand read();


private:


MachineCommand pendingCommand;

bool commandAvailable =
    false;


MachineCommand mapJogMove(
    const JogCommand& jog
);


MachineCommand mapJogCancel();


};

#endif