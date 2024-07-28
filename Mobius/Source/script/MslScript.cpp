/**
 * Not much here, but keeping it out of the .h files makes dependencies cleaner.
 */

#include <JuceHeader.h>

#include "MslModel.h"
#include "MslValue.h"
#include "MslBinding.h"
#include "MslScript.h"

MslScript::MslScript()
{
}

MslScript::~MslScript()
{
    delete root;
    // these don't cascade delete
    while (bindings != nullptr) {
        MslBinding* next = bindings->next;
        delete bindings;
        bindings = next;
    }
}

MslProc* MslScript::findProc(juce::String procname) {
    MslProc* found = nullptr;
    for (auto proc : procs) {
        if (proc->name == procname) {
            found = proc;
            break;
        }
    }
    return found;
}
