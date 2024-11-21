/**
 * Implementation of the MslWaitNode class
 * This is more complicated than the others so it was factored
 * out of MslModel.h
 *
 * A keyword is required:
 * 
 *    wait <type keyword>
 *
 * Some keywords require an amount
 *
 *    wait frame 123
 *
 * A repetition count is allowed but a few will ignore it
 *
 *    wait subcycle repeat 2
 *
 * If you are exactly on a boundary, the wait will normally end
 * immediately.  The "next" keyword can be used to force it to the next
 * boundary.
 * 
 *    wait next bar
 *    or
 *    wait bar next
 *
 * Boundary waits are normally relative to the current location.  To
 * make them relative to the start of the loop use "number"
 *
 *    wait subcycle number 2
 */

#include <JuceHeader.h>

#include "MslModel.h"
#include "MslWaitNode.h"
#include "MslParser.h"

//////////////////////////////////////////////////////////////////////
//
// Keyword Mapping
//
//////////////////////////////////////////////////////////////////////

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
    {"end",, MslWaitEnd},
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
    
    {nullptr, MslWantNone}
};

const char* MslWaitNode::typeToKeyword(MslWaitType type)
{
    const char* keyword = nullptr;
    for (int i = 0 ; MslWaitKeywordDefinitions[i] != nullptr ; i++) {
        if (MslWaitKeywordDefinitions[i].type == type) {
            keyword = MslWaitKeywordDefinitions[i].name;
            break;
        }
    }
    return keyword;
}

MslWaitType MslWaitNode::keywordToType(const char* key)
{
    MslWaitType type = MslWaitNone;
    for (int i = 0 ; MslWaitKeywordDefinitions[i] != nullptr ; i++) {
        if (strcmp(MslWaitKeywordDefinitions[i].name, key) == 0) {
            type = MslWaitKeywordDefinitions[i].type;
            break;
        }
    }
    return type;
}

//////////////////////////////////////////////////////////////////////
//
// Node Parsing
//
//////////////////////////////////////////////////////////////////////

/**
 * See file header comments for more on syntax
 *
 * This is the first one that would really bennefit from a real parser,
 * what with the optional keywords and required values and such.
 * Not worth messing with yet.
 */
bool MslWaitNode::wantsToken(MslParser* p, MslToken& t)
{
    bool wants = false;
    const char* key = t.value.toUTF8();

    if (strcmp(key, "next") == 0) {
        // allow next on either side of the type, or anywhere really
        if (next) {
            // complain about this or just ignore it?
            p->errorSyntax(t, "Duplicate next keyword");
        }
        else {
            next = true;
            wants = true;
        }
    }
    else if (type == WaitTypeNone) {
        // first one needs to be the type
        // I suppose we could let this be out of order too, but why bother
        type = keywordToType(key);
        if (type == WaitTypeNone)
          p->errorSyntax(t, "Invalid wait type");
        else {
            wants = true;
            // some of these have required amount numbers
            if (type == WaitFrame || type == WaitMsec || type == WaitSecond)
              waitingForAmount = true;
        }
    }
    else if (strcmp(key, "number")) {
        if (isWaitingForNumber())
          p->errorSyntax(t, "Misplaced keyword");
        else if (number > 0)
          p->errorSyntax(t, "Number already specified");
        else {
            waitingForNumber = true;
            wants = true;
        }
    }
    else if (strcmp(key, "repeat")) {
        if (isWaitingForNumber())
          p->errorSyntax(t, "Misplaced keyword");
        else if (repeats > 0)
          p->errorSyntax(t, "Repeat already specified");
        else {
            waitingForRepeat = true;
            wants = true;
        }
    }
    return wants;
}

bool MslWaitNode::isWaitingForNumber()
{
    return (waitingForAmount || waitingForNumber || waitingForRepeat);
}

bool MslWaitNode::wantsNode(MslParser* p, MslNode* node)
{
    (void)node;
    bool wants = false;
    if (type == WaitTypeNone) {
        p->errorSyntax(node, "Missing wait keyword");
    }
    else if (waitingForAmount) {
        amountNodeIndex = children.size();
        wants = true;
    }
    else if (waitingForNumber) {
        numberNodeIndex = children.size();
        wants = true;
    }
    else if (waitingForRepeat) {
        repeatNodeIndex = children.size();
        wants = true;
    }
    return wants;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
