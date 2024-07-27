
#include "../util/Util.h"
#include "MslModel.h"
#include "MslError.h"

MslError::MslError()
{
    init();
}

/**
 * Constructor used by the parser.
 */
MslError::MslError(int l, int c, juce::String t, juce::String d)
{
    next = nullptr;
    line = l;
    column = c;
    setToken(t.toUT8());
    setDetails(d.toUTF8())
}

/**
 * Initializer when using it in the pool.
 */
void MslError::init()
{
    next = nullptr;
    line = 0;
    column = 0;
    strcpy(token, "");
    strcpy(details, "");
}

/**
 * Initializer used by the interpreter (MslSession)
 */
void MslError::init(MslNode* node, const char* details)
{
    // okay, this shit happens a lot now, why not just standardize on passing
    // the MslToken by value everywhere
    next = nullptr;
    line = node->token.line;
    column = node->token.column;
    setToken(node->token.value.toUTF8());
    setDetails(details);
}

// no substructure on these
MslError::~MslError()
{
}

void MslError::setToken(const char* src)
{
    CopyString(src, token, sizeof(token));
}

void MslError::setDetails(const char* src)
{
    CopyString(src, details, sizeof(details));
}
