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
#include "MslDetails.h"
#include "MslCollision.h"
#include "MslResult.h"
#include "MslBinding.h"
#include "MslPreprocessor.h"
#include "ConsolePanel.h"

// not supposed to see this, but we're special
#include "MslCompilation.h"

#include "MobiusConsole.h"

MobiusConsole::MobiusConsole(Supervisor* s, ConsolePanel* parent)
{
    supervisor = s;
    panel = parent;
    
    scriptenv = supervisor->getScriptEnvironment();

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
 */
void MobiusConsole::update()
{
}

//
// Console callbacks
//

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

    else if (line.startsWith("list")) {
        doList(withoutCommand(line));
    }
    else if (line.startsWith("show") || line.startsWith("details")) {
        doDetails(withoutCommand(line));
    }
    else if (line.startsWith("load")) {
        doLoad(withoutCommand(line));
    }
    else if (line.startsWith("unload")) {
        doUnload(withoutCommand(line));
    }

    else if (line.startsWith("registry")) {
        doRegistry(withoutCommand(line));
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
    
    else if (line.startsWith("parse")) {
        doParse(withoutCommand(line));
    }
    else if (line.startsWith("preproc")) {
        doPreproc(withoutCommand(line));
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

    // contents of the environment
    console.add("list      list loaded scripts and symbols");
    console.add("show      show details of an object");
    console.add("load      load a script file");
    console.add("unload    unload a script file");

    // contents of the registry
    console.add("registry  show status of the script registry");
    
    // sessions
    console.add("status    show the status of an async session");
    console.add("resume    resume the last scriptlet after a wait");
    console.add("results   show prior evaluation results");
    
    console.add("parse     parse a line of MSL text");
    console.add("preproc   test the preprocessor");
    console.add("signature test the signature parser");
    console.add("<text>    evaluate a line of mystery");
}

//////////////////////////////////////////////////////////////////////
//
// Environment Information
//
//////////////////////////////////////////////////////////////////////

/**
 * Reload portions of the script environment.
 *
 *    load           - reload the entire MSL library
 *    load <file>    - load a file
 *
 */
void MobiusConsole::doLoad(juce::String line)
{
    ScriptClerk* clerk = supervisor->getScriptClerk();

    line = line.trim();

    if (line.length() == 0) {
        clerk->refresh();
        clerk->installMsl();
        showLoad();
    }
    else {
        MslDetails* details = clerk->loadFile(line);
        showDetails(details);
        delete details;
    }
}

void MobiusConsole::doUnload(juce::String line)
{
    console.add("Not implemented");
}

/**
 * Display details of a compilation unit in the console.
 */
void MobiusConsole::showDetails(MslDetails* details)
{
    if (details->errors.size() > 0) {
        console.add("Errors:");
        showErrors(&(details->errors));
    }
    if (details->warnings.size() > 0) {
        console.add("Warnings:");
        showErrors(&(details->warnings));
    }
    
    console.add("Details:");
    console.add("  id: " + details->id);
    console.add("  name: " + details->name);
    juce::String published ((details->published) ? "true" : "false");
    console.add("  published: " + published);

    if (details->unresolved.size() > 0) {
        console.add("Unresolved:");
        for (auto name : details->unresolved) {
            console.add("  " + name);
        }
    }

    if (details->collisions.size() > 0) {
        console.add("Collisions:");
        for (auto col : details->collisions) {
            console.add("  " + col->name + ":" + col->otherPath);
        }
    }
}

/**
 * Emit the status of the last load including errors to the console.
 *
 * todo: this is showing the REGISTRY which is similar to but not the
 * same as the loaded units in the environment.  Will want both.
 */
void MobiusConsole::showLoad()
{
    console.add("Needs work...");
#if 0    
    ScriptClerk* clerk = supervisor->getScriptClerk();
    ScriptRegistry* registry = clerk->getRegistry();
    juce::OwnedArray<class ScriptRegistry::File>* files = registry->getFiles();

    int missing = 0;
    for (auto file : *files) {
        if (file->missing)
          missing++;
    }

    if (missing > 0) {
        console.add("Missing files:");
        for (auto file : *files) {
            if (file->missing)
              console.add(file->path);
        }
    }

    console.add("Files:");
    juce::OwnedArray<class MslScriptUnit>* units = scriptenv->getUnits();
    for (auto unit : *units) {
        console.add(unit->path);
        if (unit->errors.size() > 0) {
            console.add("Errors:");
            for (auto err : unit->errors) {
                juce::String errline;
                // damn, Mac is really whiny about having char* combined with +
                // have to juce::String wrap everything, why not Windows?
                errline += juce::String("  Line ") + juce::String(err->line) + juce::String(" column ") + juce::String(err->column) +
                    juce::String(": ") +  juce::String(err->token) + juce::String(" ") + juce::String(err->details);
                console.add(errline);
            }
        }

        if (unit->collisions.size() > 0) {
            console.add("Name collisions:");
            for (auto col : unit->collisions) {
                console.add(col->name + " " + col->fromPath + " " + col->otherPath);
            }
        }
    }
#endif    
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

/**
 * List the ids of all installed compilation units.
 *
 * todo: recognize args to list other things
 */
void MobiusConsole::doList(juce::String line)
{
    juce::String type = line.trim();
    if (type == "") {
        juce::StringArray ids = scriptenv->getUnits();
        console.add(juce::String(ids.size()) + " compilation units:");
        for (auto id : ids)
          console.add("  " + id);
    }
    else if (type.startsWith("link")) {
        juce::Array<MslLinkage*> links = scriptenv->getLinks();
        console.add(juce::String(links.size()) + " links:");
        for (auto link : links) {
            juce::String details;
            if (link->function != nullptr)
              details = "function";
            else if (link->variable != nullptr)
              details = "variable";
            else
              details = "unresolved";
            console.add("Link " + link->name + ": " + details +
                        " unit: " + link->unit->id);
        }
    }
}

void MobiusConsole::doDetails(juce::String line)
{
    juce::String id = line.trim();
    MslDetails* details = scriptenv->getDetails(id);
    if (details != nullptr) {
        showDetails(details);
        delete details;
    }
}

/**
 * Display information about the locally defined
 * functions and variables in this scriptlet session.
 * Requires a backdoor into the MslCompilation unit
 * that most applications won't get.
 */
void MobiusConsole::doLocal()
{
    console.add("Not implemented");
#if 0    
    juce::OwnedArray<MslScript>* scripts = scriptenv->getScripts();
    if (scripts != nullptr && scripts->size() > 0) {
        console.add("Loaded Scripts");
        for (auto script : *scripts) {
            console.add("  " + script->name);
        }
    }
    juce::OwnedArray<MslFunctionNode>* funcs = session->getFunctions();
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
#endif    
}

void MobiusConsole::doShow(juce::String line)
{
    console.add("Not implemented");
}

void MobiusConsole::doRegistry(juce::String line)
{
    console.add("Not implemented");
}

//////////////////////////////////////////////////////////////////////
//
// Parsing and Evaluation
//
//////////////////////////////////////////////////////////////////////

void MobiusConsole::doParse(juce::String line)
{
    MslParser parser;
    MslCompilation* unit = parser.parse(line);
    if (unit != nullptr) {
        // todo: error details display
        showErrors(&(unit->errors));

        MslFunction* f = unit->getBodyFunction();
        if (f != nullptr)
          traceNode(f->getBody(), 2);
        
        delete unit;
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

void MobiusConsole::doEval(juce::String line)
{
    bool success = false;
    
    // establish a new scriptlet "unit" if we don't have one
    if (scriptlet.length() == 0) {
        MslDetails* details = scriptenv->install(supervisor, "", line);
        success = details->errors.size() == 0;
        if (details->errors.size() > 0 || details->warnings.size() > 0)
          showDetails(details);
        // remember the id for next time if it didn't fail
        scriptlet = details->id;
        delete details;
    }
    else {
        MslDetails* details = scriptenv->extend(supervisor, scriptlet, line);
        success = details->errors.size() == 0;
        if (details->errors.size() > 0 || details->warnings.size() > 0)
          showDetails(details);
        delete details;
    }

    if (success) {
        MslResult* result = scriptenv->eval(supervisor, scriptlet);
        showResult(result);
        delete result;
    }
}

void MobiusConsole::showResult(MslResult* result)
{
    showErrors(result->errors);
    showValue(result->value);
    
    asyncSession = 0;
    
    if (result->isWaiting()) {
        asyncSession = result->sessionId;
        console.add("Session " + juce::String(asyncSession) + " is waiting");
    }
    if (result->isTransitioning()) {
        asyncSession = result->sessionId;
        console.add("Session " + juce::String(asyncSession) + " is transitioning");
    }
}

void MobiusConsole::showValue(MslValue* value)
{
    if (value != nullptr)
      console.add(juce::String(value->getString()));
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
    // new: this will need a back door to get to the MslCompilation
    // either that or return it in the details
    console.add("doSignature is broken");
#if 0
    MslScript* s = session->getScript();
    if (s->arguments != nullptr) {
        console.add("Arguments:");
        traceNode(s->arguments.get(), 2);
    }
#endif    
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
            MslFunctionNode* func = static_cast<MslFunctionNode*>(node);
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
