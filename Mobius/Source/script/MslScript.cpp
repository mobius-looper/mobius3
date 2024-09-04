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

MslFunctionNode* MslScript::findFunction(juce::String fname) {
    MslFunctionNode* found = nullptr;
    for (auto func : functions) {
        if (func->name == fname) {
            found = func;
            break;
        }
    }
    return found;
}

/**
 * Vars are not gathered into a list like procs.
 */
MslVariable* MslScript::findVariable(juce::String varname) {
    MslVariable* found = nullptr;
    if (root != nullptr) {
        for (auto node : root->children) {
            if (node->isVariable()) {
                MslVariable* var = static_cast<MslVariable*>(node);
                if (var->name == varname) {
                    found = var;
                    break;
                }
            }
        }
    }
    return found;
}
