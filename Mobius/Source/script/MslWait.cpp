/**
 * Implementation of the MslWait used to pass wait requests to the MslContainer
 * may not need this if the model is simple enough?
 */

#include <JuceHeader.h>

#include "MslModel.h"

class MslWaitKeywordDefinition {
  public:
    const char* name;
    MslWaitType type;
};

MslWaitKeywordDefinition MslWaitKeywordDefinitions[] = {

    {"none", MslWaitNone},
    {"subcycle", MslWaitSubcycle},
    {"cycle", MslWaitCycle},
    // ambiguous whether this should mean start or end
    {"loop", MslWaitStart},
    {"start", MslWaitStart},
    {"end", MslWaitEnd},
    {"beat", MslWaitBeat},
    {"bar", MslWaitBar},
    {"marker", MslWaitMarker},

    // since these are always used with a nuber
    // let them be pluralized
    {"frame", MslWaitFrame},
    {"frames", MslWaitFrame},
    {"msec", MslWaitMsec},
    {"msecs", MslWaitMsec},
    {"second",MslWaitSecond},
    {"seconds",MslWaitSecond},

    {"block", MslWaitBlock},
    {"last", MslWaitLast},
    {"switch", MslWaitSwitch},
    {"externalStart", MslWaitExternalStart},
    {"pulse", MslWaitPulse},
    {"realign", MslWaitRealign},
    {"return", MslWaitReturn},
    {"driftCheck", MslWaitDriftCheck},
    
    {nullptr, MslWaitNone}
};

const char* MslWait::typeToKeyword(MslWaitType t)
{
    const char* keyword = nullptr;
    for (int i = 0 ; MslWaitKeywordDefinitions[i].name != nullptr ; i++) {
        if (MslWaitKeywordDefinitions[i].type == t) {
            keyword = MslWaitKeywordDefinitions[i].name;
            break;
        }
    }
    return keyword;
}

MslWaitType MslWait::keywordToType(const char* key)
{
    MslWaitType wtype = MslWaitNone;
    for (int i = 0 ; MslWaitKeywordDefinitions[i].name != nullptr ; i++) {
        if (strcmp(MslWaitKeywordDefinitions[i].name, key) == 0) {
            wtype = MslWaitKeywordDefinitions[i].type;
            break;
        }
    }
    return wtype;
}

