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

            // some of these have required amount numbers
            
        }
    }
    else if (type == WaitTypeNone) {
        // first one needs to be the type
        type = keywordToType(key);
        if (type != WaitTypeNone)
          wants = true;
        else
          p->errorSyntax(t, "Invalid wait type");
    }
    else if (strcmp(key, "number")) {
        if (waitingForAnyNumber())
          p->errorSyntax(t, "Misplaced keyword");
        else if (number > 0)
          p->errorSyntax(t, "Number already specified");
        else {
            waitingForNumber = true;
            wants = true;
        }
    }
    else if (strcmp(key, "repeat")) {
        if (waitingForAnyNumber())
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

bool MslWaitNode::wantsNode(MslParser* p, MslNode* node)
{
    (void)node;
    bool wants = false;
    if (type == WaitTypeNone) {
        p->errorSyntax(node, "Missing wait keyword");
    }
    else if (waitingForAnyNumber()) {
        wants = true;
    }
    else 
    

    
    return (children.size() < 1);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
