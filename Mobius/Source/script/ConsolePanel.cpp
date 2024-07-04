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
    else if (line.startsWith("trace")) {
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

void MobiusConsole::testParse(juce::String line)
{
    // skip over "trace"
}

void MobiusConsole::doLine(juce::String line)
{
    // todo: allow parsing into a stack object
    MslNode* node = parser.parse(line);
    if (node == nullptr) {
        juce::StringArray errors = parser.getErrors();
        for (auto error : errors) {
            console.add(error);
        }
    }
    else {
        juce::String result = evaluator.start(node);
        if (result.length() > 0)
          console.add(result);

        juce::StringArray errors = evaluator.getErrors();
        for (auto error : errors) {
            console.add(error);
        }
        
        delete node;
    }
}



/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
