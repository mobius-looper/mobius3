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
 *
 * Boundary waits are normally relative to the current location.  To
 * make them relative to the start of the loop use "number"
 *
 *    wait subcycle number 2
 */

#include <JuceHeader.h>

#include "MslModel.h"
#include "MslParser.h"

//////////////////////////////////////////////////////////////////////
//
// Keyword Mapping
//
//////////////////////////////////////////////////////////////////////

const char* MslWaitTypeKeywords[] = {
    "none",
    "subcycle",
    "cycle",
    "loop",
    "start",
    "end",
    "beat",
    "bar",
    "marker",
    "frame",
    "msec",
    "second",
    "block",
    "last",
    "switch"
    "externalStart",
    "pulse",
    "realign",
    "return",
    "driftCheck",
    nullptr
    
};

/**
 * Don't just trust these as indexes.
 */
const char* MslWaitNode::enumToKeyword(const char* keywords[], int e)
{
    const char* keyword = nullptr;
    for (int i = 0 ; i <= e ; i++) {
        keyword = keywords[i];
        if (keyword == nullptr)
          break;
    }
    return keyword;
}

int MslWaitNode::keywordToEnum(const char* keywords[], const char* key)
{
    int e = 0;
    for (int i = 0 ; keywords[i] != nullptr ; i++) {
        if (strcmp(keywords[i], key) == 0) {
            e = i;
            break;
        }
    }
    return e;
}

// enum specific mappers
// used to have more of these, now there is only Type

MslWaitType MslWaitNode::keywordToType(const char* s)
{
    return (MslWaitType)keywordToEnum(MslWaitTypeKeywords, s);
}

const char* MslWaitNode::typeToKeyword(MslWaitType e)
{
    return enumToKeyword(MslWaitTypeKeywords, e);
}

//////////////////////////////////////////////////////////////////////
//
// Node Parsing
//
//////////////////////////////////////////////////////////////////////

/**
 * See file header comments for more on syntax
 */
bool MslWaitNode::wantsToken(MslParser* p, MslToken& t)
{
    bool wants = false;
    const char* key = t.value.toUTF8();

    // I suppose we could accept the two keywords in any order
    // and juggle their meaning?
    // hmm, might be able to collapse the enumerations and
    // sort out what is nonsensical later
    
    if (type == WaitTypeNone) {
        type = keywordToType(key);
        if (type != WaitTypeNone)
          wants = true;
        else {
            // implicit type=event if the token matches an event keyword
            // these are the most common and have priority
            MslWaitEvent ev = keywordToEvent(key);
            if (ev != WaitEventNone) {
                type = WaitTypeEvent;
                event = ev;
                wants = true;
            }
            else {
                // implicit type=duration or type=location if the token has
                // an unambiguous match
                MslWaitDuration dur = keywordToDuration(key);
                MslWaitLocation loc = keywordToLocation(key);
                if (dur != WaitDurationNone && loc == WaitLocationNone) {
                    type = WaitTypeDuration;
                    duration = dur;
                    wants = true;
                }
                else if (dur == WaitDurationNone && loc != WaitLocationNone) {
                    type = WaitTypeLocation;
                    location = loc;
                    wants = true;
                }
                else if (dur == WaitDurationNone && loc == WaitLocationNone) {
                    p->errorSyntax(t, "Invalid wait unit");
                }
                else {
                    p->errorSyntax(t, "Ambiguous wait unit: use location or duration");
                }
            }
        }
    }
    else if (type == WaitTypeEvent && event == WaitEventNone) {
        event = keywordToEvent(key);
        if (event != WaitEventNone)
          wants = true;
        else
          p->errorSyntax(t, "Invalid event name");
    }
    else if (type == WaitTypeDuration && duration == WaitDurationNone) {
        duration = keywordToDuration(key);
        if (duration != WaitDurationNone)
          wants = true;
        else
          p->errorSyntax(t, "Invalid duration name");
    }
    else if (type == WaitTypeLocation && location == WaitLocationNone) {
        location = keywordToLocation(key);
        if (location != WaitLocationNone)
          wants = true;
        else
          p->errorSyntax(t, "Invalid location name");
    }
    
    return wants;
}

/**
 * Accept one expression node as an event count,
 * location number, or duration length.
 *
 * ugh, some combos don't need arguments
 *   wait last
 *
 * and some are rare to have multipliers
 *   wait loop
 *
 * "argument" list is possible
 *
 *    wait loop(2)
 *
 * or a different keyword
 *
 *    waitn loop 2
 *
 * or require an argument list
 *
 *    wait(loop 2)
 *
 * None are pretty
 */
bool MslWaitNode::wantsNode(MslNode* node)
{
    (void)node;
    return (children.size() < 1);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
