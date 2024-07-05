/**
 * A testing panel that shows an interactive console.
 * 
 */

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../ui/JuceUtil.h"

#include "MslModel.h"
#include "ConsolePanel.h"

MobiusConsole::MobiusConsole(ConsolePanel* parent)
{
    panel = parent;
    addAndMakeVisible(&console);
    console.setListener(this);
    
    console.add("Shall we play a game?");
    console.prompt();
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
    parseLine(line);
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

void MobiusConsole::parseLine(juce::String line)
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
        if (evaluator.trace) {
            console.add("Trace disabled");
            evaluator.trace = false;
        }
        else {
            console.add("Trace enabled");
            evaluator.trace = true;
        }
    }
    else if (line.startsWith("parse")) {
        testParse(line);
    }
    else {
        doLine(line);
    }
}

void MobiusConsole::showHelp()
{
    console.add("?         help");
    console.add("clear     clear display");
    console.add("quit      close the console");
    console.add("trace     toggle trace mode");
    console.add("parse     parse a line of script");
    console.add("foo       evaluate a line of mystery");
}

void MobiusConsole::showErrors(juce::StringArray* errors)
{
    for (auto error : *errors)
      console.add(error);
}

void MobiusConsole::doLine(juce::String line)
{
    // todo: allow parsing into a stack object
    MslNode* node = parser.parse(line);
    if (node == nullptr) {
        showErrors(parser.getErrors());
    }
    else {
        MslValue result = evaluator.start(node);
        const char* s = result.getString();
        if (s != nullptr) 
          console.add(s);

        showErrors(evaluator.getErrors());
        
        delete node;
    }
}

void MobiusConsole::testParse(juce::String line)
{
    juce::String remainder = line.fromFirstOccurrenceOf("parse", false, false);
    MslNode* node = parser.parse(remainder);
    if (node == nullptr) {
        showErrors(parser.getErrors());
    }
    else {
        traceNode(node, 0);
        delete node;
    }
}

void MobiusConsole::traceNode(MslNode* node, int indent)
{
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

    console.add(line);

    for (auto child : node->children)
      traceNode(child, indent + 2);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
