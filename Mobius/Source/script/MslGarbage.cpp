
#include <JuceHeader.h>

#include "MslCompilation.h"
#include "MslGarbage.h"

MslGarbage::MslGarbage(MslPools* p)
{
    pool = p;
}

/**
 * Reclaim anything in the trash
 * It is the responsibility of the caller to ensure that
 * there are no active MslSessions and nothing else needs what
 * is in here.
 */
void MslGarbage::flush()
{
    // units are not pooled
    units.clear();

    // neither are blocks 
    blocks.clear();
}

MslGarbage::~MslGarbage()
{
    // contents are in OwnedArrays so if we have any left
    // they delete themselves
}
