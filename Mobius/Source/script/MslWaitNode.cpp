/**
 * Implementation of the MslWaitNode class
 * This is more complicated than the others so it was factored
 * out of MslModel.h
 *
 * Very old-school on the name mapping here.  Explore something better.
 * Since this is only used at parse time can make use of juce::HashMap
 * or something.  The assumption is that the index of each string in the
 * array matches the numeric value of the corresponding enumeration.
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
    "event",
    "duration",
    "location",
    nullptr
};

const char* MslWaitEventKeywords[] = {
    "none",
    "loop",
    "end",
    "cycle",
    "subcycle",
    "beat",
    "bar",
    "marker",
    "last",
    "switch",
    "block",
    "externalStart",
    "pulse",
    "realign",
    "return",
    "driftCheck",
    nullptr
};

const char* MslWaitDurationKeywords[] = {
    "none",
    "frame",
    "msec",
    "second",
    "subcycle",
    "cycle",
    "loop",
    "beat",
    "bar",
    nullptr
};

const char* MslWaitLocationKeywords[] = {
    "none",
    "start",
    "end",
    "subcycle",
    "cycle",
    "beat",
    "bar",
    "marker",
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
// Type

MslWaitType MslWaitNode::keywordToType(const char* s)
{
    return (MslWaitType)keywordToEnum(MslWaitTypeKeywords, s);
}

const char* MslWaitNode::typeToKeyword(MslWaitType e)
{
    return enumToKeyword(MslWaitTypeKeywords, e);
}

// Event

MslWaitEvent MslWaitNode::keywordToEvent(const char* s)
{
    return (MslWaitEvent)keywordToEnum(MslWaitEventKeywords, s);
}

const char* MslWaitNode::eventToKeyword(MslWaitEvent e)
{
    return enumToKeyword(MslWaitEventKeywords, e);
}

// Duration

MslWaitDuration MslWaitNode::keywordToDuration(const char* s)
{
    return (MslWaitDuration)keywordToEnum(MslWaitDurationKeywords, s);
}

const char* MslWaitNode::durationToKeyword(MslWaitDuration e)
{
    return enumToKeyword(MslWaitDurationKeywords, e);
}

// Location

MslWaitLocation MslWaitNode::keywordToLocation(const char* s)
{
    return (MslWaitLocation)keywordToEnum(MslWaitLocationKeywords, s);
}

const char* MslWaitNode::locationToKeyword(MslWaitLocation e)
{
    return enumToKeyword(MslWaitLocationKeywords, e);
}

//////////////////////////////////////////////////////////////////////
//
// Node Parsing
//
//////////////////////////////////////////////////////////////////////

/**
 * General structure is
 *
 * Wait <type> stuff...
 *
 * Wait event <eventType> <eventCount-expression>
 *
 * Wait duration <durationType> <durationLength-expression>
 *
 * Wait location <locationType> <locationNumber-expression>
 *
 * For syntax convenience, if the event wait token is one of the
 * event type names asume it is an event type.  The conflicts are
 * on things like "subcycle" which is used for all three types, but
 * event waits are the most common.  Reconsider this...
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
 */
bool MslWaitNode::wantsNode(MslNode* node)
{
    (void)node;
    return (children.size() < 1);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
