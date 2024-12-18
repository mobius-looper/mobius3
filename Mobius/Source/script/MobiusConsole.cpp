/**
 * The interactive MSL console.
 */

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../util/Util.h"
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

    else if (line.startsWith("status")) {
        doStatus(withoutCommand(line));
    }
    else if (line.startsWith("result")) {
        doResults(withoutCommand(line));
    }
    else if (line.startsWith("proc")) {
        doProcesses(withoutCommand(line));
    }
    else if (line.startsWith("diag")) {
        doDiagnostics(withoutCommand(line));
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
    else if (line.startsWith("namespace")) {
        doNamespace(withoutCommand(line));
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
    console.add("?            help");
    console.add("clear        clear display");
    console.add("quit         close the console");
    console.add("");
    // contents of the environment
    console.add("list         list exported links");
    console.add("list units   list compilation units");
    console.add("list files   list script registry files");
    console.add("show <id>    show details of a compilation unit");
    console.add("load <path>  load a script file");
    console.add("unload <id>  unload a compilation unit");
    console.add("namespace    change namespaces");
    console.add("");
    // sessions
    console.add("status       show the status of an async session");
    console.add("resume       resume the last scriptlet after a wait");
    console.add("results      show prior evaluation results");
    console.add("processes    show current processes");
    console.add("diagnostics  enable/disable extended diagnostics");
    console.add("");
    console.add("parse        parse a line of MSL text");
    console.add("preproc      test the preprocessor");
    console.add("signature    test the signature parser");
    console.add("<text>       evaluate a line of mystery");
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
    console.add("Unit: " + details->id);
    console.add("Reference name: " + details->name);
    juce::String published ((details->published) ? "true" : "false");
    console.add("Published: " + published);
    if (details->unresolved.size() > 0) {
        console.add("Unresolved:");
        for (auto name : details->unresolved) {
            console.add("  " + name);
        }
    }
    if (details->collisions.size() > 0) {
        console.add("Collisions:");
        for (auto col : details->collisions) {
            console.add("  \"" + col->name + "\" with " + col->otherPath);
        }
    }
    if (details->errors.size() > 0) {
        console.add("Errors:");
        showErrors(&(details->errors));
    }
    if (details->warnings.size() > 0) {
        console.add("Warnings:");
        showErrors(&(details->warnings));
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
 * List the ids of all installed compilation units or
 * the list of exported links for all units.
 */
void MobiusConsole::doList(juce::String line)
{
    juce::String ltype = line.trim();

    if (ltype == "") {
        console.add("list links | units | files");
        ltype = "link";
    }
        
    if (ltype.startsWith("unit")) {
        console.add("Compilation Units");
        juce::StringArray ids = scriptenv->getUnits();
        int number = 1;
        for (auto id : ids) {
            console.add(juce::String(number) + ": " + id);
            number++;
        }
    }
    else if (ltype.startsWith("link")) {
        console.add("Exported Links");
        juce::Array<MslLinkage*> links = scriptenv->getLinks();
        for (auto link : links) {
            juce::String type;
            if (link->function != nullptr)
              type = "function";
            else if (link->variable != nullptr)
              type = "variable";
            else
              type = "unresolved";
            juce::String unit;
            if (link->unit == nullptr)
              unit = "unloaded";
            else
              unit = link->unit->id;
            
            // was kida hoping tab would work, but it's a variable width font so no
            console.add(link->name + " \t" +
                        " type=" + type + 
                        " unit=" + unit);
        }
    }
    else if (ltype.startsWith("reg") || ltype.startsWith("file") || ltype.startsWith("lib")) {
        console.add("Script Library");
        ScriptClerk* clerk = supervisor->getScriptClerk();
        ScriptRegistry* reg = clerk->getRegistry();
        ScriptRegistry::Machine* machine = reg->getMachine();
        for (auto file : machine->files) {
            console.add("File: " + file->path + "  \"" + file->name + "\"");
            //console.add("  reference name: " + file->name);
            juce::String options;
            if (file->missing) options += "missing ";
            if (file->disabled) options += "disabled ";
            if (file->external != nullptr) options += "external ";
            if (file->button) options += "button ";
            if (file->library) options += "library ";
            if (options.length() > 0)
              console.add("  flags: " + options);
            MslDetails* details = file->getDetails();
            if (details != nullptr && details->hasErrors())
              console.add("  Has Errors");
        }
        if (machine->externals.size() > 0) {
            console.add("External File Registry");
            for (auto ext : machine->externals) {
                console.add("External: " + ext->path);
            }
        }
    }
}

void MobiusConsole::doDetails(juce::String line)
{
    juce::String id = line.trim();

    // hack: if you type #n then use that as an index to the id list
    // as returned by doList, since typing paths is annoying
    if (IsInteger(id.toUTF8())) {
        int index = id.getIntValue() - 1;
        juce::StringArray ids = scriptenv->getUnits();
        if (index >= 0 && index < ids.size())
          id = ids[index];
        else {
            console.add(juce::String("Invalid unit list number.  Must be between 1 and ") + juce::String(ids.size()));
            id = "";
        }
    }

    if (id.length() > 0) {
        MslDetails* details = scriptenv->getDetails(id);
        if (details != nullptr) {
            showDetails(details);
            delete details;
        }
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

//////////////////////////////////////////////////////////////////////
//
// Parsing and Evaluation
//
//////////////////////////////////////////////////////////////////////

void MobiusConsole::doNamespace(juce::String line)
{
    juce::String arg = line.trim();

    if (arg.length() == 0) {
        juce::String ns = scriptenv->getNamespace(scriptlet);
        if (ns.length() == 0)
          ns = "global";
        console.add(ns);
    }
    else {
        juce::String error = scriptenv->setNamespace(scriptlet, arg);
        if (error.length() > 0)
          console.add(error);
    }
}

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
    // uses the special variableCarryover option so we can define variables
    // and reference them from one console line to the next
    // todo: need a way to list and clear this list
    // applies to any static or otherwise scoped variable
    if (scriptlet.length() == 0)
      scriptlet = scriptenv->registerScriptlet(supervisor, true);

    MslDetails* details = scriptenv->extend(supervisor, scriptlet, line);
    success = details->errors.size() == 0;
    if (details->errors.size() > 0 || details->warnings.size() > 0)
      showDetails(details);
    delete details;

    if (success) {
        MslResult* result = scriptenv->eval(supervisor, scriptlet);
        showResult(result);
        delete result;
    }
}

void MobiusConsole::showResult(MslResult* result)
{
    if (result != nullptr) {
        showErrors(result->errors);
        showValue(result->value);
    
        asyncSession = 0;
    
        if (result->state == MslStateWaiting) {
            asyncSession = result->sessionId;
            console.add("Session " + juce::String(asyncSession) + " is waiting");
        }
        if (result->state == MslStateTransitioning) {
            asyncSession = result->sessionId;
            console.add("Session " + juce::String(asyncSession) + " is transitioning");
        }
    }
}

void MobiusConsole::showValue(MslValue* value)
{
    if (value != nullptr) {
        if (value->type == MslValue::List || value->list != nullptr) {
            juce::String lvalue;
            for (MslValue* v = value->list ; v != nullptr ; v = v->next) {
                if (lvalue.length() == 0)
                  lvalue += "[";
                else
                  lvalue += " ";
                lvalue += v->getString();
            }
            lvalue += "]";
            console.add(lvalue);
        }
        else {
            console.add(juce::String(value->getString()));
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

void MobiusConsole::doProcesses(juce::String arg)
{
    if (arg.length() == 0) {
        juce::Array<MslProcess> result;
        scriptenv->listProcesses(result);
        for (int i = 0 ; i < result.size() ; i++) {
            MslProcess& p = result.getReference(i);

            juce::String status;
            status += p.name;

            switch (p.state) {
                case MslStateNone: status += " no status"; break;
                case MslStateFinished: status += " finished"; break;
                case MslStateError: status += " errors"; break;
                case MslStateRunning: status += " running"; break;
                case MslStateWaiting: status += " waiting"; break;
                case MslStateSuspended: status += " suspended"; break;
                case MslStateTransitioning: status += " transitioning"; break;
            }

            console.add(juce::String(p.sessionId) + ": " + status);
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

void MobiusConsole::doDiagnostics(juce::String arg)
{
    (void)arg;
    bool current = scriptenv->isDiagnosticMode();
    if (current)
      console.add("Diagnostic mode is off");
    else
      console.add("Diagnostic mode is on");
    scriptenv->setDiagnosticMode(!current);
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
            MslLiteralNode* l = node->getLiteral();
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
            MslVariableNode* var = node->getVariable();
            line += "Variable: " + var->name;
            // todo: any node should be able to have properties?
            if (var->properties.size() > 0) {
                console.add(line);
                for (auto prop : var->properties)
                  traceNode(prop, indent + 4);
                line = "";
            }
        }
        else if (node->isProperty()) {
            MslPropertyNode* prop = node->getProperty();
            line += "Property: " + prop->token.value;
        }
        else if (node->isForm()) {
            MslFormNode* form = node->getForm();
            line += "Form: " + form->name;
        }
        else if (node->isField()) {
            MslFieldNode* field = node->getField();
            line += "Field: " + field->name;
            // todo: any node should be able to have properties?
            if (field->properties.size() > 0) {
                console.add(line);
                for (auto prop : field->properties)
                  traceNode(prop, indent + 4);
                line = "";
            }
        }
        else if (node->isFunction()) {
            MslFunctionNode* func = node->getFunction();
            line += "Function: " + func->name;
        }
        else if (node->isIf()) {
            line += "If: ";
        }
        else if (node->isElse()) {
            line += "Else: ";
        }
        else if (node->isReference()) {
            MslReferenceNode* ref = node->getReference();
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
            MslWaitNode* waitnode = node->getWait();
            line += "Wait: " + juce::String(MslWait::typeToKeyword(waitnode->type));
        }
        else if (node->isContext()) {
            MslContextNode* con = node->getContext();
            line += "Context: " + juce::String(con->shell ? "shell" : "kernel");
        }
        else if (node->isKeyword()) {
            MslKeywordNode* key = node->getKeyword();
            line += "Keyword: " + key->name;
        }
        else if (node->isCase()) {
            line += "Case";
        }
        else {
            line += "???: ";
        }

        if (line.length() > 0)
          console.add(line);

        for (auto child : node->children)
          traceNode(child, indent + 2);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
