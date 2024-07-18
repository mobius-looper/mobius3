/**
 * The interactive MSL console.
 * 
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
#include "ConsolePanel.h"
#include "MobiusConsole.h"

MobiusConsole::MobiusConsole(ConsolePanel* parent)
{
    panel = parent;
    addAndMakeVisible(&console);
    console.setListener(this);
    
    console.add("Shall we play a game?");
    console.prompt();

    // get a more convenient handle to the script environment
    scriptenv = Supervisor::Instance->getScriptEnvironment();

    // allocate a scriptlet session we can keep forever
    session = new MslScriptletSession(scriptenv);

    // todo: need to work out the info passing
    //session.setListener(this);
}

MobiusConsole::~MobiusConsole()
{
    // really need more encapsulation of this
    delete session;
}

void MobiusConsole::showing()
{
    // don't reset this every time, it's more convenient to hide/show it
    // and have it remember what you were doing
    //console.clear();
}

void MobiusConsole::hiding()
{
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
    console.add("<text>    evaluate a line of mystery");
}

void MobiusConsole::doLoad(juce::String line)
{
    if (line.startsWith("config")) {
        scriptenv->loadConfig();
    }
    else {
        scriptenv->resetLoad();
        scriptenv->load(line);
    }

    showLoad();
}

/**
 * Emit the status of the environment, including errors to the console.
 */
void MobiusConsole::showLoad()
{
    juce::StringArray& missing = scriptenv->getMissingFiles();
    if (missing.size() > 0) {
        console.add("Missing files:");
        for (auto s : missing) {
            console.add(s);
        }
    }

    juce::OwnedArray<class MslFileErrors>* ferrors = scriptenv->getFileErrors();
    if (ferrors->size() > 0) {
        console.add("File errors:");
        for (auto mfe : *ferrors) {
            console.add(mfe->path);
            for (auto err : mfe->errors) {
                juce::String errline;
                errline += "  Line " + juce::String(err->line) + " column " + juce::String(err->column) +
                    err->token + " " + err->details;;
                console.add(errline);
            }
        }
    }

    juce::OwnedArray<class MslCollision>* collisions = scriptenv->getCollisions();
    if (collisions->size() > 0) {
        console.add("Name Collisions:");
        for (auto col : *collisions) {
            console.add(col->name + " " + col->fromPath + " " + col->otherPath);
        }
    }

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
    MslParserResult* res = parser.parse(line);
    if (res != nullptr) {
        // todo: error details display
        showErrors(res);

        if (res->script != nullptr)
          traceNode(res->script->root, 0);

        delete res;
    }
}

void MobiusConsole::showErrors(MslParserResult* res)
{
    for (auto error : res->errors) {
        juce::String msg = "Line " + juce::String(error->line) +
            " column " + juce::String(error->column) + ": " +
            error->token + ": " + error->details;
        console.add(msg);
    }
}

void MobiusConsole::doList()
{
}
 
void MobiusConsole::doEval(juce::String line)
{
    session->eval(line);

    // todo: here is where we display the result of the evaluation and
    // any runtime errors
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
              line += "Int: " + node->token;
            else if (l->isFloat)
              line += "Float: " + node->token;
            else if (l->isBool)
              line += "Bool: " + node->token;
            else
              line += "String: " + node->token;
        }
        else if (node->isSymbol()) {
            line += "Symbol: " + node->token;
        }
        else if (node->isBlock()) {
            line += "Block: " + node->token;
        }
        else if (node->isOperator()) {
            line += "Operator: " + node->token;
        }
        else if (node->isAssignment()) {
            line += "Assignment: " + node->token;
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
