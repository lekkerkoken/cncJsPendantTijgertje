#ifndef MACHINE_MAPPER_H
#define MACHINE_MAPPER_H

#include "JogCommand.h"
#include "MachineCommand.h"

class MachineMapper
{
public:

    MachineCommand map(
        const JogCommand& jog
    );

private:

    MachineCommand mapJogMove(
        const JogCommand& jog
    );

};

#endif