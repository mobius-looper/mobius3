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

/**
 * Vars are not gathered into a list like procs.
 */
MslVar* MslScript::findVar(juce::String varname) {
    MslVar* found = nullptr;
    if (root != nullptr) {
        for (auto node : root->children) {
            if (node->isVar()) {
                MslVar* var = static_cast<MslVar*>(node);
                if (var->name == varname) {
                    found = var;
                    break;
                }
            }
        }
    }
    return found;
}
