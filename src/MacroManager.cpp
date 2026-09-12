#include "MacroManager.h"


// ============================================================
// CLEAR
// ============================================================

void MacroManager::clear()
{
    for(
        size_t i = 0;
        i < count_;
        ++i
    )
    {
        macros_[i].id =
            "";

        macros_[i].name =
            "";

        macros_[i].content =
            "";
    }


    count_ =
        0;
}


// ============================================================
// SET MACROS
// ============================================================

bool MacroManager::setMacros(
    const MacroInfo* macros,
    size_t count
)
{
    if(
        macros == nullptr &&
        count > 0
    )
    {
        return false;
    }


    if(
        count > MAX_MACROS
    )
    {
        return false;
    }


    clear();


    for(
        size_t i = 0;
        i < count;
        ++i
    )
    {
        macros_[i] =
            macros[i];
    }


    count_ =
        count;


    return true;
}


// ============================================================
// COUNT
// ============================================================

size_t MacroManager::count() const
{
    return count_;
}


// ============================================================
// GET
// ============================================================

const MacroInfo*
MacroManager::get(
    size_t index
) const
{
    if(
        index >= count_
    )
    {
        return nullptr;
    }


    return &macros_[index];
}