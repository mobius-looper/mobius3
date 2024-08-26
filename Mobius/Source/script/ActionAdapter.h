/**
 * Utility class that does runtime model mapping between
 * the UIAction/Query models used by the Mobius application
 * and the MslEnvironment models.
 *
 * This is suitable for temporary stack allocation, it retains no state.
 */

#pragma once

#include <JuceHeader.h>

class ActionAdapter
{
  public:

    ActionAdapter() {}
    ~ActionAdapter() {}

    void doAction(class MslEnvironment* env, class MslContext* c, class UIAction* action);

};
    
