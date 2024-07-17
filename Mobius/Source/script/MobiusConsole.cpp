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
#include "MslEnvironment.h"
#include "ConsolePanel.h"
#include "MobiusConsole.h"

MobiusConsole::MobiusConsole(ConsolePanel* parent)
{
    panel = parent;
    addAndMakeVisible(&console);
    console.setListener(this);
    
    console.add("Shall we play a game?");
    console.prompt();

    session.setListener(this);

    // get a more convenient handle to the script environment
    scriptenv = Supervisor::Instance->getScriptEnvironment();
}

MobiusConsole::~MobiusConsole()
{
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
        showHelp();
    }
    else if (line == "clear") {
        console.clear();
    }
    else if (line == "quit" || line == "exit") {
        panel->close();
    }
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
    else if (line.startsWith("parse")) {
        testParse(line);
    }
    else if (line.startsWith("load")) {
        loadFile(line);
    }
    else if (line.startsWith("list")) {
        listSymbols();
    }
    else {
        eval(line);
    }
}

void MobiusConsole::showHelp()
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

void MobiusConsole::showErrors(juce::StringArray& errors)
{
    for (auto error : errors)
      console.add(error);
}

juce::String MobiusConsole::withoutCommand(juce::String line)
{
    return line.fromFirstOccurrenceOf(" ", false, false);
}

void MobiusConsole::loadFile(juce::String line)
{
    juce::StringArray files;
    files.add(withoutCommand(line));
    
    scriptenv->loadFiles(files);

    showErrors(scriptenv->getErrors());

    MslParserResult* result = scriptenv->getLastResult();
    if (result != nullptr) {
        showErrors(result->errors);
    }
}

void MobiusConsole::testParse(juce::String line)
{
    MslParserResult* res = parser.parse(withoutCommand(line));
    if (res != nullptr) {
        // todo: error details display
        showErrors(res->errors);

        if (res->script != nullptr)
          traceNode(res->script->root, 0);

        delete res;
    }
}

void MobiusConsole::listSymbols()
{
    console.add("Loaded Scripts:");
    for (auto script : *(scriptenv->getScripts()))
      console.add("  " + script->name);
    
    // formerly dumped the "session"
    // might still be intersting but I want to test files for awhile
#if 0    
    juce::OwnedArray<MslProc>* procs = session.getProcs();
    if (procs->size() > 0) {
        console.add("Procs:");
        for (auto proc : *procs) {
            console.add("  " + proc->name);
        }
    }
#endif    
}
 
void MobiusConsole::eval(juce::String line)
{
    // session does most of it's information convenyance throught the listener
    session.eval(line);
}

void MobiusConsole::mslError(const char* s)
{
    console.add(juce::String(s));
}

void MobiusConsole::mslTrace(const char* s)
{
    console.add(juce::String(s));
}

void MobiusConsole::mslResult(const char* s)
{
    console.add(juce::String(s));
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
