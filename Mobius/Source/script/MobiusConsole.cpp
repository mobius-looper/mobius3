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
#include "MslScriptlet.h"
#include "MslResult.h"
#include "MslBinding.h"
#include "MslFileError.h"
#include "MslCollision.h"
#include "MslPreprocessor.h"
#include "ConsolePanel.h"
#include "MobiusConsole.h"

// !! need a Supervisor link, either at construction
// or during a later initialization
// tried to make this only require an MslContext but there is too much
// going on here to be completely independent

MobiusConsole::MobiusConsole(Supervisor* s, ConsolePanel* parent)
{
    supervisor = s;
    panel = parent;
    
    scriptenv = supervisor->getScriptEnvironment();

    // allocate a scriptlet session we can keep forever
    session = scriptenv->newScriptlet();
    session->setName("Console");
    
    addAndMakeVisible(&console);
    console.setListener(this);
    console.add("Shall we play a game?");
    console.prompt();
}

MobiusConsole::~MobiusConsole()
{
    // environment owns the Scriptlet
    supervisor->removeMobiusConsole(this);
}

void MobiusConsole::showing()
{
    // don't reset this every time, it's more convenient to hide/show it
    // and have it remember what you were doing
    //console.clear();

    // install ourselves as a sort of listener on the Supervisor to receive
    // forwarded mslPrint calls when the scriptlet is pushed into the background
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

void MobiusConsole::mslPrint(const char* msg)
{
    console.add(msg);
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
    else if (line.startsWith("preproc")) {
        doPreproc(withoutCommand(line));
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
    else if (line.startsWith("signature")) {
        doSignature();
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
 * Reload portions of the script configuration.
 *
 *    load           - reload the MSL library
 *    load <file>    - load a file
 *    load config    - reload the ScriptConfig
 *
 */
void MobiusConsole::doLoad(juce::String line)
{
    ScriptClerk* clerk = supervisor->getScriptClerk();

    line = line.trim();

    if (line.length() == 0) {
        clerk->loadLibrary();
    }
    else if (line.startsWith("config")) {
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
                // damn, Mac is really whiny about having char* combined with +
                // have to juce::String wrap everything, why not Windows?
                errline += juce::String("  Line ") + juce::String(err->line) + juce::String(" column ") + juce::String(err->column) +
                    juce::String(": ") +  juce::String(err->token) + juce::String(" ") + juce::String(err->details);
                console.add(errline);
            }
        }
    }

    // this just passes through to MslEnvironment so we can get it there too
    // it isn't the collisions on just this file yet
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
    MslScript* result = parser.parse(line);
    if (result != nullptr) {
        // todo: error details display
        showErrors(&(result->errors));

        if (result->root != nullptr)
          traceNode(result->root, 0);
        
        delete result;
    }
}

void MobiusConsole::doPreproc(juce::String line)
{
    MslPreprocessor preproc;

    juce::File root = supervisor->getRoot();
    juce::File file = root.getChildFile(line);
    if (!file.existsAsFile()) {
        console.add("File does not exist: " + juce::String(line));
    }
    else {
        juce::String src = file.loadFileAsString();
        juce::String result = preproc.process(src);
        console.add("Preprocessor results:");
        console.add(result);
    }
}

// show shell-level errors maintained in an OwnedArray
void MobiusConsole::showErrors(juce::OwnedArray<MslError>* errors)
{
    for (auto error : *errors) {
        juce::String msg = "Line " + juce::String(error->line) +
            juce::String(" column ") + juce::String(error->column) + juce::String(": ") +
            juce::String(error->token) + juce::String(": ") + juce::String(error->details);
        console.add(msg);
    }
}

// show kernel-level errors maintained in a linked list
void MobiusConsole::showErrors(MslError* list)
{
    for (MslError* error = list ; error != nullptr ; error = error->next) {
        juce::String msg = "Line " + juce::String(error->line) +
            juce::String(" column ") + juce::String(error->column) + juce::String(": ") +
            juce::String(error->token) + juce::String(": ") + juce::String(error->details);
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
    juce::OwnedArray<MslFunction>* funcs = session->getFunctions();
    if (funcs != nullptr && funcs->size() > 0) {
        console.add("Defined Functions");
        for (auto func : *funcs) {
            console.add("  " + func->name);
        }
    }
    MslBinding* bindings = session->getBindings();
    if (bindings != nullptr) {
        console.add("Defined Variables");
        while (bindings != nullptr) {
            console.add("  " + juce::String(bindings->name));
            bindings = bindings->next;
        }
    }
}
 
void MobiusConsole::doEval(juce::String line)
{
    if (!session->compile(supervisor, line)) {
        juce::OwnedArray<MslError>* errors = session->getCompileErrors();
        if (errors != nullptr)
          showErrors(errors);
    }
    else {
    
        (void)session->eval(supervisor);

        // side effect of an evaluation is a list of errors and a result
        MslError* errors = session->getErrors();
        if (errors != nullptr) {
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
        MslResult* r = scriptenv->getResult(id);
        if (r == nullptr)
          console.add("Session " + juce::String(id) + " not found");
        else {
            if (r->value != nullptr)
              console.add("Session " + juce::String(id) + " finished with " +
                          juce::String(r->value->getString()));
            else
              console.add("Session " + juce::String(id) + " finished with no result");
        }
    }
}

void MobiusConsole::doResume()
{
    // we used to intercept Waits and simulate them
    // what we do need now is commands that can operate on waiting scripts
    // to cancel wait states and terminate the script
    console.add("Resume not implemented");
}

void MobiusConsole::doResults(juce::String arg)
{
    if (arg.length() == 0) {
        MslResult* result = scriptenv->getResults();
        while (result != nullptr) {
            juce::String status;
            status += result->name;
            if (!result->isRunning()) status += " finished";
            if (result->isWaiting()) status += " waiting";
            if (result->errors != nullptr) status += " errors";

            console.add(juce::String(result->sessionId) + ": " + status);
            result = result->getNext();
        }
    }
    else {
        int id = arg.getIntValue();
        if (id > 0) {
            MslResult* result = scriptenv->getResult(id);
            if (result == nullptr) {
                console.add("No results for session " + juce::String(id));
            }
            else {
                console.add("Session " + juce::String(id) + " " + juce::String(result->name));
                showErrors(result->errors);
                MslValue* value = result->value;
                if (value != nullptr)
                  console.add("Result value: " + juce::String(value->getString()));
                else
                  console.add("No result value");
            }
        }
    }
}

/**
 * Test hack for directive parsing
 * # directives are parsed into the scriptlet's MslScript like other statements
 * but since they do not evaluate we have no way to look at them.
 */
void MobiusConsole::doSignature()
{
    MslScript* s = session->getScript();
    if (s->arguments != nullptr) {
        console.add("Arguments:");
        traceNode(s->arguments.get(), 2);
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
        else if (node->isVariable()) {
            MslVariable* var = static_cast<MslVariable*>(node);
            line += "Variable: " + var->name;
        }
        else if (node->isFunction()) {
            MslFunction* func = static_cast<MslFunction*>(node);
            line += "Function: " + func->name;
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
        else if (node->isPrint()) {
            line += "Print";
        }
        else if (node->isIn()) {
            line += "In";
        }
        else if (node->isSequence()) {
            line += "Sequence";
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
        else if (node->isKeyword()) {
            MslKeyword* key = static_cast<MslKeyword*>(node);
            line += "Keyword: " + key->name;
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
