#ifndef MACRO_MANAGER_H
#define MACRO_MANAGER_H

#include <Arduino.h>

#include "MacroInfo.h"


class MacroManager
{
public:

    // ========================================================
    // CONSTANTS
    // ========================================================

    static constexpr size_t MAX_MACROS = 8;


    // ========================================================
    // LIFECYCLE
    // ========================================================

    void clear();


    // ========================================================
    // DATA
    // ========================================================

    bool setMacros(
        const MacroInfo* macros,
        size_t count
    );

    size_t count() const;

    const MacroInfo* get(
        size_t index
    ) const;


private:

    MacroInfo macros_[MAX_MACROS];

    size_t count_ =
        0;
};


#endif