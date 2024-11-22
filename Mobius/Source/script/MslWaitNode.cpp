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
#include "MslWait.h"
#include "MslParser.h"

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
    else if (type == MslWaitNone) {
        // first one needs to be the type
        // I suppose we could let this be out of order too, but why bother
        type = MslWait::keywordToType(key);
        if (type == MslWaitNone)
          p->errorSyntax(t, "Invalid wait type");
        else {
            wants = true;
            // some of these have required amount numbers
            if (type == MslWaitFrame || type == MslWaitMsec || type == MslWaitSecond)
              waitingForAmount = true;
        }
    }
    else if (strcmp(key, "number") == 0) {
        if (isWaitingForNumber())
          p->errorSyntax(t, "Misplaced keyword");
        else if (numberNodeIndex >= 0)
          p->errorSyntax(t, "Number already specified");
        else {
            waitingForNumber = true;
            wants = true;
        }
    }
    else if (strcmp(key, "repeat") == 0) {
        if (isWaitingForNumber())
          p->errorSyntax(t, "Misplaced keyword");
        else if (repeatNodeIndex >= 0)
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
    if (type == MslWaitNone) {
        p->errorSyntax(node, "Missing wait keyword");
    }
    else if (waitingForAmount) {
        amountNodeIndex = children.size();
        waitingForAmount = false;
        wants = true;
    }
    else if (waitingForNumber) {
        numberNodeIndex = children.size();
        waitingForNumber = false;
        wants = true;
    }
    else if (waitingForRepeat) {
        repeatNodeIndex = children.size();
        waitingForRepeat = false;
        wants = true;
    }
    return wants;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
