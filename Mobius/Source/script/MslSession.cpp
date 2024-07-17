/**
 * State for an interactive MSL session.
 *
 * Need to improve conveyance of errors.  To be useful the messages
 * need to have line/column numbers and it would support a syntax highlighter
 * better if the error would include that in an MslError model rather than
 * embedded in the text.
 */

#include <JuceHeader.h>

#include "../model/UIAction.h"
#include "../model/UIParameter.h"
#include "../model/Query.h"
#include "../model/Symbol.h"
#include "../model/ScriptProperties.h"
#include "../Supervisor.h"

#include "MslSession.h"

MslSession::MslSession()
{
    // prepare the parser with our dynamic script
    parser.prepare(&dynamicScript);
}

MslSession::~MslSession()
{
}

void MslSession::conveyErrors(juce::StringArray* elist)
{
    for (auto error : *elist)
      listener->mslError(error.toUTF8());
}

/**
 * Consume a block of MSL text, immediately evaluate what was parsed,
 * and display the result.  Accumuluate proc and var definitions.
 *
 * This is starting to become a horrible interface.  Revisit after file loading works.
 */
void MslSession::eval(juce::String src)
{
    MslParserResult* pres = parser.consume(src);

    conveyErrors(&(pres->errors));
    
    MslBlock* root = dynamicScript.root;
    for (int i = 0 ; i < root->size(); i++) {

        MslNode* node = root->get(i);
        if (node != nullptr) {

            // sigh, three levels of error list: session, parser, evaluator
            // think about consolidating this
            errors.clear();
            
            MslValue result = evaluator.start(this, node);

            conveyErrors(evaluator.getErrors());
            conveyErrors(&errors);
        
            const char* s = result.getString();
            if (s != nullptr) {
                if (listener != nullptr)
                  listener->mslResult(s);
            }
        }
    }

    delete pres;
}

// old dead code
#if 0
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

#endif

//////////////////////////////////////////////////////////////////////
//
// Symbol Resolution
//
// Unclear whether this should go in MslEvaluator or in Session
// but until it becomes clearer how this should work keep it up here
// and keep teh parser clean.
//
//////////////////////////////////////////////////////////////////////

/**
 * Resolve a symbol reference in a part tree by decorating it with the
 * thing that it references.
 * Returns false if it could not be resolved.
 */
bool MslSession::resolve(MslSymbol* snode)
{
    bool resolved = false;

    // first check for cached symbol
    if (snode->symbol == nullptr && snode->proc == nullptr) {

        // look for a proc
        snode->proc = dynamicScript.findProc(snode->token);
        if (snode->proc == nullptr) {
            // todo: resolve vars in the current scope

            // else resolve to a global symbol
            snode->symbol = Symbols.find(snode->token);
        }
    }
    
    resolved = (snode->symbol != nullptr || snode->proc != nullptr);
    
    return resolved;
}

/**
 * Evaluate a Symbol node from the the parse tree and leave the result.
 * Not sure I like the handoff between resolve and eval...
 */
void MslSession::eval(MslSymbol* snode, MslValue& result)
{
    result.setNull();

    if (!resolve(snode)) {
        errors.add("Unresolved symbol " + snode->token);
    }
    else {
        if (snode->proc != nullptr) {
            // todo: arguments
            // kludge, result passing sucks, we've bounced from the Evaulator
            // up to the Session, and now back down to Evaluator which is
            // where result came from, and will be set as a side effect

            // sigh
            // evaluating a proc means evaluating it's body
            // proc nodes just have a child list, not a block, or do they?
            MslBlock* body = snode->proc->getBody();
            if (body != nullptr)
              evaluator.mslVisit(body);
        }
        else if (snode->symbol != nullptr) {
            Symbol* s = snode->symbol;
            if (s->function != nullptr) {

                invoke(s, result);
            }
            else if (s->parameter != nullptr) {
        
                query(s, result);
            }
        }
    }
}

void MslSession::invoke(Symbol* s, MslValue& result)
{
    UIAction a;
    a.symbol = s;
    // todo: arguments
    // todo: this needs to take a reference
    Supervisor::Instance->doAction(&a);

    // what is the result of a function?
    result.setNull();
}

void MslSession::query(Symbol* s, MslValue& result)
{
    result.setNull();

    if (s->parameter == nullptr) {
        errors.add("Error: Not a parameter symbol " + s->name);
    }
    else {
        Query q;
        q.symbol = s;
        bool success = Supervisor::Instance->doQuery(&q);
        if (!success) {
            errors.add("Error: Unable to query parameter " + s->name);
        }
        else if (q.async) {
            // not really an error, need a different message/warning list
            errors.add("Asynchronous parameter query " + s->name);
        }
        else {
            // And now we have the issue of whether to return an ordinal
            // or a label.  At runtime you usually want an ordinal, in the
            // interactive console usually a label.
            // will need a syntax for that, maybe ordinal(foo) or foo.ordinal

            UIParameterType ptype = s->parameter->type;
            if (ptype == TypeEnum) {
                // don't use labels since I want scripters to get used to the names
                //result.setString(s->parameter->getEnumLabel(q.value));
                result.setString(s->parameter->getEnumName(q.value));
            }
            else if (ptype == TypeBool) {
                result.setBool(q.value == 1);
            }
            else if (ptype == TypeStructure) {
                // hmm, the understand of LevelUI symbols that live in
                // UIConfig and LevelCore symbols that live in MobiusConfig
                // is in Supervisor right now
                // todo: Need to repackage this
                result.setJString(Supervisor::Instance->getParameterLabel(s, q.value));
            }
            else {
                // should only be here for TypeInt
                // unclear what String would do
                result.setInt(q.value);
            }
        }
    }
}

void MslSession::assign(MslSymbol* snode, int value)
{
    resolve(snode);
    if (snode->symbol != nullptr) {
        // todo: context forwarding
        UIAction a;
        a.symbol = snode->symbol;
        a.value = value;
        Supervisor::Instance->doAction(&a);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
