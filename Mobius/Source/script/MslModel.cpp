/**
 * Implementations for MslNode methods that need to have their
 * definitions hidden to avoid adding gratuituous dependencies on
 * everything that includes MslModel.h.  Mostly this is MslParser.h
 * used to inject error messages during token parsing.
 */

#include "MslModel.h"
#include "MslParser.h"

bool MslReference::wantsToken(MslParser* p, MslToken& t)
{
    bool wants = false;
    if (name.length() == 0) {
        if (t.type == MslToken::Type::Symbol ||
            t.type == MslToken::Type::Int) {
            name = t.value;
            wants = true;
        }
        else {
            p->errorSyntax(t, "Invalid reference");
        }
    }
    return wants;
}

bool MslKeyword::wantsToken(MslParser* p, MslToken& t)
{
    bool wants = false;
    if (name.length() == 0) {
        if (t.type == MslToken::Type::Symbol) {
            name = t.value;
            wants = true;
        }
        else {
            p->errorSyntax(t, "Invalid keyword");
            // todo: could also check this against the set of known keywords
        }
    }
    return wants;
}

/**
 * var is one of the few that consumes tokens
 * hmm, it's a little more than this, it REQUIRES a token
 * wantsToken doesn't have a way to reject with prejeduce
 * we'll end up with a bad parse tree that will have to be caught at runtime
 * update: added error returns in MslParser
 */
bool MslVariable::wantsToken(MslParser* p, MslToken& t)
{
    (void)p;
    bool wants = false;
    if (name.length() == 0) {
        if (t.type == MslToken::Type::Symbol) {
            // take this as our name
            name =  t.value;
            wants = true;
        }
    }
    else if (t.type == MslToken::Type::Operator &&
             t.value == "=") {
        // skip past this once we have a name
        wants = true;
    }
    else {
        // now that we can stick errors in MslParser, is this
        // where that should go?
    }
    return wants;
}

bool MslFunctionNode::wantsToken(MslParser* p, MslToken& t)
{
    (void)p;
    bool wants = false;
    if (name.length() == 0) {
        if (t.type == MslToken::Type::Symbol) {
            name =  t.value;
            wants = true;
        }
    }
    return wants;
}

bool MslContextNode::wantsToken(MslParser* p, MslToken& t)
{
    bool wants = false;
    if (!finished) {
        if (t.type == MslToken::Type::Symbol) {
            if (t.value == "shell" || t.value == "ui") {
                shell = true;
                finished = true;
            }
            else if (t.value == "kernel" || t.value == "audio") {
                shell = false;
                finished = true;
            }
        }
        if (finished)
          wants = true;
        else {
            p->errorSyntax(t, "Invalid context name");
        }
    }
    return wants;
}
