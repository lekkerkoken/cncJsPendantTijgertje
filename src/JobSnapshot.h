#ifndef JOB_SNAPSHOT_H
#define JOB_SNAPSHOT_H

#include <Arduino.h>

struct JobSnapshot
{
    bool valid = false;
    String name;

    void invalidate()
    {
        valid = false;
        name = "";
    }
};
#endif