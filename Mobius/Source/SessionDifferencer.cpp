
#include <JuceHeader.h>

#include "model/Session.h"
#include "model/ValueSet.h"
#include "model/SessionDiff.h"
#include "model/Symbol.h"

#include "Provider.h"

#include "SessionDifferencer.h"


SessionDiffs* SessionDifferencer::diff(Provider* p, Session* original, Session* modified)
{
    SessionDiffs* diffs = new SessionDiffs();

    diffValueSet(p, diffs, original->ensureGlobals(), modified->ensureGlobals());
}

void SessionDifferencer::diffValueSet(Provider* p, SessionDiffs* diffs, ValueSet* src, ValueSet* neu)
{
    SymbolTable* symbols = p->getSymbols();
    
    juce::StringArray keys;
    src->getKeys(keys);
    for (auto key : keys) {
        Symbol* s = symbols->find(key);
        if (s == nullptr) {
            Trace(1, "SessionDifferencer: Invalid symbol key %s", key.toUTF8());
        }
        else {
            MslValue* srcv = src->get(key);
            MslValue* neuv = neu->get(key);

            if (neuv == nullptr) {
                // this one was deleted
            

    
}
