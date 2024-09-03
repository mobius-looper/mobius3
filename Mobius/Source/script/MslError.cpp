
#include <JuceHeader.h>

#include "../util/Util.h"
#include "MslModel.h"
#include "MslError.h"

MslError::MslError()
{
    init();
}

MslError::MslError(MslError* src)
{
    init();
    line = src->line;
    column = src->column;
    setToken(src->token);
    setDetails(src->details);
}

/**
 * Constructor used by the parser.
 */
MslError::MslError(int l, int c, juce::String t, juce::String d)
{
    next = nullptr;
    line = l;
    column = c;
    setToken(t.toUTF8());
    setDetails(d.toUTF8());
}

/**
 * Constructor used by the linker.
 */
MslError::MslError(MslNode* node, juce::String d)
{
    init(node, d.toUTF8());
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
void MslError::init(MslNode* node, const char* deets)
{
    // okay, this shit happens a lot now, why not just standardize on passing
    // the MslToken by value everywhere
    next = nullptr;
    line = node->token.line;
    column = node->token.column;
    setToken(node->token.value.toUTF8());
    setDetails(deets);
}

/**
 * No substructure on these but they cascade delete.
 */
MslError::~MslError()
{
    delete next;
}

void MslError::setToken(const char* src)
{
    CopyString(src, token, sizeof(token));
}

void MslError::setDetails(const char* src)
{
    CopyString(src, details, sizeof(details));
}
