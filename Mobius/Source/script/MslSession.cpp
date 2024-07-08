/**
 * State for an interactive MSL session.
 *
 * Need to improve conveyance of errors.  To be useful the messages
 * need to have line/column numbers and it would support a syntax highlighter
 * better if the error would include that in an MslError model rather than
 * embedded in the text.
 */

#include <JuceHeader.h>

#include "../model/Symbol.h"
#include "../model/ScriptProperties.h"

#include "MslSession.h"

/** 
 * Main entry point to parse and evaluate one script
 */
void MslSession::eval(juce::String src)
{
    MslNode* node = parser.parse(src);
    if (node == nullptr) {
        juce::StringArray* parseErrors = parser.getErrors();
        if (listener != nullptr) {
            for (auto error : *parseErrors)
              listener->mslError(error.toUTF8());
        }
    }
    else {
        // install procs and evaluate directives
        node = assimilate(node);

        // at this point, or maybe during the assimulation walk,
        // we could have a "link" phase where we resolve symbol
        // references in the parsed scriptlet to local procs and vars

        // evaluation will run to the end unless there is a Wait
        // encountered
        // will need a way to pass back suspension state which means
        // the value here isn't relevant yet
        MslValue result = evaluator.start(this, node);

        juce::StringArray* evalErrors = evaluator.getErrors();
        if (listener != nullptr) {
            for (auto error : *evalErrors)
              listener->mslError(error.toUTF8());
        }
        
        const char* s = result.getString();
        if (s != nullptr) {
            if (listener != nullptr)
              listener->mslResult(s);
        }

        delete node;
    }
        
}

/**
 * Walk over a parse tree, removing proc nodes and installing them in
 * the session proc table.
 * Not handling scoped proc, a proc at any level will be interned.
 * Scoped, or nested proc definitiosn are allowed but any sort of temporary
 * block-level definition makes it much harder for the interactive console
 * to deal with them.
 *
 * We might as well handle directives here too.
 */
MslNode* MslSession::assimilate(MslNode* node)
{
    if (node->isProc()) {
        MslProc* proc = static_cast<MslProc*>(node);
        intern(proc);
        node = nullptr;
    }
    else {
        // unclear if iterators tolerate modification while active
        int index = 0;
        while (index < node->children.size()) {
            MslNode* child = node->children[index];
            if (assimilate(child) != nullptr) {
                // did not consume it
                index++;
            }
        }
    }
    return node;
}

/**
 * Intern a proc from a new scriptlet parse tree
 */
void MslSession::intern(MslProc* proc)
{
    proc->detach();
    MslProc* existing = procTable[proc->name];
    if (existing != nullptr) {
        procs.remove(existing, false, false);
        delete existing;
    }
    procs.add(proc);
    procTable.set(proc->name, proc);
    
    Symbol* s = symbols.intern(proc->name);
    ScriptProperties* sprop = s->script.get();
    if (sprop == nullprt) {
        sprop = new ScriptProperties();
        s->script = sprop;
    }
    sprop->proc = proc;
}

Symbol* MslSession::findSymbol(juce::String name)
{
    return symbols.get(name);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
