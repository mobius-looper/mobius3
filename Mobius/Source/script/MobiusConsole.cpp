/**
 * The interactive MSL console.
 */

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../ui/JuceUtil.h"
#include "../Supervisor.h"

#include "MslModel.h"
#include "MslParser.h"
#include "MslError.h"
#include "MslEnvironment.h"
#include "MslScriptletSession.h"
#include "MslSession.h"
#include "ConsolePanel.h"
#include "MobiusConsole.h"

// !! need a Supervisor link, either at construction
// or during a later initialization
// tried to make this only require an MslContext but there is too much
// going on here to be completely independent

MobiusConsole::MobiusConsole(ConsolePanel* parent)
{
    panel = parent;

    // pass this down!
    supervisor = Supervisor::Instance;
    scriptenv = supervisor->getScriptEnvironment();

    // allocate a scriptlet session we can keep forever
    session = scriptenv->newScriptletSession();
    
    addAndMakeVisible(&console);
    console.setListener(this);
    console.add("Shall we play a game?");
    console.prompt();
}

MobiusConsole::~MobiusConsole()
{
    // environment owns the ScriptletSession
    supervisor->removeMobiusConsole(this);
}

void MobiusConsole::showing()
{
    // don't reset this every time, it's more convenient to hide/show it
    // and have it remember what you were doing
    //console.clear();

    // install ourselves as a sort of listener on the Supervisor to receive
    // forwarded mslEcho calls when the scriptlet is pushed into the background
    // and advanced on the maintenance thread
    supervisor->addMobiusConsole(this);
}

void MobiusConsole::hiding()
{
    supervisor->removeMobiusConsole(this);
}

void MobiusConsole::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    
    console.setBounds(area);
}

void MobiusConsole::paint(juce::Graphics& g)
{
    (void)g;
}

void MobiusConsole::buttonClicked(juce::Button* b)
{
    (void)b;
}

/**
 * Called during Supervisor's advance() in the maintenance thread.
 * Refresh the whole damn thing if anything changes.
 */
void MobiusConsole::update()
{
}

void MobiusConsole::consoleLine(juce::String line)
{
    doLine(line);
}

void MobiusConsole::consoleEscape()
{
    panel->close();
}

//
// MslContext implementations
// hard wired Supervisor references till we can sort that shit out
//

MslContextId MobiusConsole::mslGetContextId()
{
    return MslContextShell;
}

juce::File MobiusConsole::mslGetRoot()
{
    return supervisor->getRoot();
}

MobiusConfig* MobiusConsole::mslGetMobiusConfig()
{
    return supervisor->getMobiusConfig();
}

void MobiusConsole::mslAction(UIAction* a)
{
    supervisor->doAction(a);
}

bool MobiusConsole::mslQuery(Query* q)
{
    return supervisor->doQuery(q);
}

bool MobiusConsole::mslWait(MslWait* w)
{
    wait = w;
    // pretend this worked
    // this keeps it out of both Supervisor and MobiusKernel so we can
    // simulate wait handling, hmm, well no, as soon
    // as transitioning works, the session the scriptlet launches
    // is going to go over to the kernel so we can't really keep a handle
    // to this
    return true;
}

/**
 * This is the only real reason we implement this, to get
 * echo statements from the script session into the console.
 */
void MobiusConsole::mslEcho(const char* msg)
{
    console.add(juce::String(msg));
}

//////////////////////////////////////////////////////////////////////
//
// Commands
//
//////////////////////////////////////////////////////////////////////

void MobiusConsole::doLine(juce::String line)
{
    if (line == "?") {
        doHelp();
    }
    else if (line == "clear") {
        console.clear();
    }
    else if (line == "quit" || line == "exit") {
        panel->close();
    }
#if 0    
    else if (line == "trace") {
        if (session.trace) {
            console.add("Trace disabled");
            session.trace = false;
        }
        else {
            console.add("Trace enabled");
            session.trace = true;
        }
    }
#endif
    else if (line.startsWith("parse")) {
        doParse(withoutCommand(line));
    }
    else if (line.startsWith("load")) {
        doLoad(withoutCommand(line));
    }
    else if (line.startsWith("list")) {
        doList();
    }
    else if (line.startsWith("status")) {
        doStatus(withoutCommand(line));
    }
    else if (line.startsWith("results")) {
        doResults(withoutCommand(line));
    }
    else if (line.startsWith("resume")) {
        doResume();
    }
    else {
        doEval(line);
    }
}

juce::String MobiusConsole::withoutCommand(juce::String line)
{
    return line.fromFirstOccurrenceOf(" ", false, false);
}

void MobiusConsole::doHelp()
{
    console.add("?         help");
    console.add("clear     clear display");
    console.add("quit      close the console");
    console.add("trace     toggle trace mode");
    console.add("load      load a script file");
    console.add("parse     parse a line of script");
    console.add("list      list loaded scripts and symbols");
    console.add("show      show proc structure");
    console.add("status    show the status of an async session");
    console.add("resume    resume the last scriptlet after a wait");
    console.add("results   show prior evaluation results");
    console.add("<text>    evaluate a line of mystery");
}

/**
 * Reload the entire ScriptConfig or load an individual file.
 */
void MobiusConsole::doLoad(juce::String line)
{
    ScriptClerk* clerk = supervisor->getScriptClerk();
    
    if (line.startsWith("config")) {
        clerk->reload();
    }
    else {
        clerk->resetLoadResults();
        clerk->loadFile(line);
    }

    showLoad();
}

/**
 * Emit the status of the last load including errors to the console.
 */
void MobiusConsole::showLoad()
{
    ScriptClerk* clerk = supervisor->getScriptClerk();
    
    juce::StringArray& missing = clerk->getMissingFiles();
    if (missing.size() > 0) {
        console.add("Missing files:");
        for (auto s : missing) {
            console.add(s);
        }
    }

    juce::OwnedArray<class MslFileErrors>* ferrors = clerk->getFileErrors();
    if (ferrors->size() > 0) {
        console.add("File errors:");
        for (auto mfe : *ferrors) {
            console.add(mfe->path);
            for (auto err : mfe->errors) {
                juce::String errline;
                errline += "  Line " + juce::String(err->line) + " column " + juce::String(err->column) + ": " +  err->token + " " + err->details;;
                console.add(errline);
            }
        }
    }

    juce::OwnedArray<class MslCollision>* collisions = clerk->getCollisions();
    if (collisions->size() > 0) {
        console.add("Name Collisions:");
        for (auto col : *collisions) {
            console.add(col->name + " " + col->fromPath + " " + col->otherPath);
        }
    }

    // the actual scripts that were loaded come from the environment
    juce::OwnedArray<class MslScript>* scripts = scriptenv->getScripts();
    if (scripts->size() > 0) {
        console.add("Scripts:");
        for (auto s : *scripts) {
            console.add(s->name + " " + s->path);
        }
    }

}

void MobiusConsole::doParse(juce::String line)
{
    MslParser parser;
    MslParserResult* res = parser.parse(line);
    if (res != nullptr) {
        // todo: error details display
        showErrors(&(res->errors));

        if (res->script != nullptr)
          traceNode(res->script->root, 0);

        delete res;
    }
}

void MobiusConsole::showErrors(juce::OwnedArray<MslError>* errors)
{
    for (auto error : *errors) {
        juce::String msg = "Line " + juce::String(error->line) +
            " column " + juce::String(error->column) + ": " +
            error->token + ": " + error->details;
        console.add(msg);
    }
}

void MobiusConsole::doList()
{
    juce::OwnedArray<MslScript>* scripts = scriptenv->getScripts();
    if (scripts != nullptr && scripts->size() > 0) {
        console.add("Loaded Scripts");
        for (auto script : *scripts) {
            console.add("  " + script->name);
        }
    }
    juce::OwnedArray<MslProc>* procs = session->getProcs();
    if (procs != nullptr && procs->size() > 0) {
        console.add("Defined Procs");
        for (auto proc : *procs) {
            console.add("  " + proc->name);
        }
    }
    MslBinding* bindings = session->getBindings();
    if (bindings != nullptr) {
        console.add("Defined Vars");
        while (bindings != nullptr) {
            console.add("  " + juce::String(bindings->name));
            bindings = bindings->next;
        }
    }
}
 
void MobiusConsole::doEval(juce::String line)
{
    if (wait != nullptr) {
        console.add("Session is waiting, must be resumed");
    }
    else {
        if (!session->compile(line)) {
            MslParserResult *pr = session->getCompileErrors();
            if (pr != nullptr)
              showErrors(&(pr->errors));
        }
        else {
    
            (void)session->eval(this);

            // side effect of an evaluation is a list of errors and a result
            juce::OwnedArray<MslError>* errors = session->getErrors();
            if (errors->size() > 0) {
                showErrors(errors);
            }
            else {
                // temporary debugging
                bool fullResult = false;
                if (fullResult) {
                    juce::String full = session->getFullResult();
                    console.add(full);
                }
                else {
                    MslValue* v = session->getResult();
                    if (v != nullptr)
                      console.add(juce::String(v->getString()));
                }
            }

            asyncSession = 0;
            if (session->isWaiting()) {
                asyncSession = session->getSessionId();
                console.add("Session " + juce::String(asyncSession) + " is waiting");
            }
            if (session->isTransitioning()) {
                asyncSession = session->getSessionId();
                console.add("Session " + juce::String(asyncSession) + " is transitioning");
            }
        }
    }
}

void MobiusConsole::doStatus(juce::String line)
{
    int id = asyncSession;
    if (line.length() > 0)
      id = line.getIntValue();

    // !! the result session list is unstable,
    // need to work out how we're going to look at results and still
    // be able to prune them, some kind of lock/unlock or checkout/checkin
    if (scriptenv->isWaiting(id)) {
        console.add("Session " + juce::String(id) + " is still waiting");
    }
    else {
        MslSession* s = scriptenv->getFinished(id);
        if (s == nullptr)
          console.add("Session " + juce::String(id) + " not found");
        else {
            MslValue* r = s->getResult();
            if (r != nullptr)
              console.add("Session " + juce::String(id) + " finished with " +
                          juce::String(r->getString()));
            else
              console.add("Session " + juce::String(id) + " finished with no result");
        }
    }
}

void MobiusConsole::doResume()
{
    if (wait == nullptr)
      console.add("Session is not waiting");
    else {
        // all we can do is set the flag, it will be processed on the
        // next maintenance cycle
        wait->finished = true;
        wait = nullptr;
    }
}

void MobiusConsole::doResults(juce::String arg)
{
    if (arg.length() == 0) {
        MslSession* results = scriptenv->getResults();
        while (results != nullptr) {
            juce::String status;
            juce::OwnedArray<class MslError>* errors = results->getErrors();
            if (errors->size() > 0)
              status = "error";
            console.add(juce::String(results->getSessionId()) + " " + status);
            results = results->getNext();
        }
    }
    else {
        int id = arg.getIntValue();
        if (id > 0) {
            MslSession* s = scriptenv->getFinished(id);
            if (s == nullptr) {
                console.add("No results for session " + juce::String(id));
            }
            else {
                console.add("Session " + juce::String(id));
                juce::OwnedArray<class MslError>* errors = s->getErrors();
                showErrors(errors);
                MslValue* r = s->getResult();
                if (r != nullptr)
                  console.add("Result: " + juce::String(r->getString()));
                else
                  console.add("No results");
            }
        }
    }
}

/**
 * This needs to be packaged better into a utliity that is closer to the
 * nodes, and uses a visitor, and abstracts away the console dependency.
 */
void MobiusConsole::traceNode(MslNode* node, int indent)
{
    if (node != nullptr) {

        juce::String line;
        for (int i = 0 ; i < indent ; i++) line += " ";

        if (node->isLiteral()) {
            MslLiteral* l = static_cast<MslLiteral*>(node);
            if (l->isInt)
              line += "Int: " + node->token.value;
            else if (l->isFloat)
              line += "Float: " + node->token.value;
            else if (l->isBool)
              line += "Bool: " + node->token.value;
            else
              line += "String: " + node->token.value;
        }
        else if (node->isSymbol()) {
            line += "Symbol: " + node->token.value;
        }
        else if (node->isBlock()) {
            line += "Block: " + node->token.value;
        }
        else if (node->isOperator()) {
            line += "Operator: " + node->token.value;
        }
        else if (node->isAssignment()) {
            line += "Assignment: " + node->token.value;
        }
        else if (node->isVar()) {
            MslVar* var = static_cast<MslVar*>(node);
            line += "Var: " + var->name;
        }
        else if (node->isProc()) {
            MslProc* proc = static_cast<MslProc*>(node);
            line += "Proc: " + proc->name;
        }
        else if (node->isIf()) {
            //MslIf* ifnode = static_cast<MslIf*>(node);
            line += "If: ";
        }
        else if (node->isElse()) {
            line += "Else: ";
        }
        else if (node->isReference()) {
            MslReference* ref = static_cast<MslReference*>(node);
            line += "Reference: " + ref->name;
        }
        else if (node->isEnd()) {
            line += "End";
        }
        else if (node->isEcho()) {
            line += "Echo";
        }
        else if (node->isWait()) {
            MslWaitNode* waitnode = static_cast<MslWaitNode*>(node);
            line += "Wait: " + juce::String(waitnode->typeToKeyword(waitnode->type));
            if (waitnode->type == WaitTypeEvent)
              line += " " + juce::String(waitnode->eventToKeyword(waitnode->event));
            else if (waitnode->type == WaitTypeDuration)
              line += " " + juce::String(waitnode->durationToKeyword(waitnode->duration));
            else if (waitnode->type == WaitTypeLocation)
              line += " " + juce::String(waitnode->locationToKeyword(waitnode->location));
              
        }
        else if (node->isContext()) {
            MslContextNode* con = static_cast<MslContextNode*>(node);
            line += "Context: " + juce::String(con->shell ? "shell" : "kernel");
        }
        else {
            line += "???: ";
        }

        console.add(line);

        for (auto child : node->children)
          traceNode(child, indent + 2);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
