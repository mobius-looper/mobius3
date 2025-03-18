/**
 * The interactive MCL console.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/Util.h"

#include "../../mcl/MclModel.h"

#include "../JuceUtil.h"
#include "../../Supervisor.h"

#include "MclPanel.h"

#include "MclConsole.h"

MclConsole::MclConsole(Supervisor* s, MclPanel* parent)
{
    supervisor = s;
    panel = parent;
    
    addAndMakeVisible(&console);
    console.setListener(this);
    console.add("Shall we play another game?");
    console.prompt();
}

MclConsole::~MclConsole()
{
}

void MclConsole::showing()
{
}

void MclConsole::hiding()
{
}

void MclConsole::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    
    console.setBounds(area);
}

void MclConsole::paint(juce::Graphics& g)
{
    (void)g;
}

void MclConsole::buttonClicked(juce::Button* b)
{
    (void)b;
}

/**
 * Called during Supervisor's advance() in the maintenance thread.
 */
void MclConsole::update()
{
}

//
// Console callbacks
//

void MclConsole::consoleLine(juce::String line)
{
    doLine(line);
}

void MclConsole::consoleEscape()
{
    panel->close();
}

//
// Commands
//
//////////////////////////////////////////////////////////////////////

void MclConsole::doLine(juce::String line)
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
    else {
        doEval(line);
    }
}

juce::String MclConsole::withoutCommand(juce::String line)
{
    return line.fromFirstOccurrenceOf(" ", false, false);
}

void MclConsole::doHelp()
{
    console.add("?            help");
    console.add("clear        clear display");
    console.add("quit         close the console");
    console.add("");
    // contents of the environment
    console.add("<text>       evaluate a line of mystery");
}

//////////////////////////////////////////////////////////////////////
//
// Line Execution
//
//////////////////////////////////////////////////////////////////////

void MclConsole::doEval(juce::String line)
{
    (void)line;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
