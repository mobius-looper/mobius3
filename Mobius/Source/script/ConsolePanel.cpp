/**
 * A testing panel that shows an interactive console.
 * 
 */

#include <JuceHeader.h>

#include "../Supervisor.h"
#include "../util/Trace.h"
#include "../ui/JuceUtil.h"

#include "../model/UIAction.h"
#include "../model/Query.h"

#include "ConsolePanel.h"

MobiusConsole::MobiusConsole(ConsolePanel* parent)
{
    panel = parent;
    addAndMakeVisible(&console);
    console.setListener(this);
}

MobiusConsole::~MobiusConsole()
{
}

void MobiusConsole::showing()
{
    console.clear();
    console.add("Shall we play a game?");
    console.prompt();
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
    else {
        doLine(line);
    }
}

void MobiusConsole::showHelp()
{
    console.add("?         help");
    console.add("clear     clear display");
    console.add("test      run a test");
    console.add("foo       command of mystery");
    console.add("quit      close the console");
}

void MobiusConsole::doLine(juce::String line)
{
    tokenizer.setContent(line);

    Token t = tokenizer.next();
    if (t.isSymbol()) {
        juce::String result = eval(t);
        if (result.length() > 0)
          console.add(result);
    }
    else {
        console.add("???");
    }
}

juce::String MobiusConsole::eval(Token& t)
{
    juce::String result;
    
    Symbol* s = Symbols.find(t.value);
    if (s != nullptr)
      result = eval(s);
    else
      console.add("Unknown symbol " + t.value);

    return result;
}

juce::String MobiusConsole::eval(Symbol* s)
{
    juce::String result;
    
    if (s->function != nullptr) {
        result = invoke(s);
    }
    else if (s->parameter != nullptr) {
        result = query(s);
    }
    return result;
}

juce::String MobiusConsole::invoke(Symbol* s)
{
    UIAction a;
    a.symbol = s;
    // todo: arguments
    // todo: this needs to take a reference
    Supervisor::Instance->doAction(&a);

    // what is the result of a function?
    return juce::String("");
}

// will need more flexibility on value types
juce::String MobiusConsole::query(Symbol* s)
{
    juce::String result;

    if (s->parameter == nullptr) {
        console.add("Error: Not a parameter symbol " + s->name);
    }
    else {
        Query q;
        q.symbol = s;
        bool success = Supervisor::Instance->doQuery(&q);
        if (!success) {
            console.add("Error: Unable to query parameter " + s->name);
        }
        else if (q.async) {
            console.add("Asynchronous parameter query " + s->name);
        }
        else {
            // And now we have the issue of whether to return an ordinal
            // or a label.  At runtime you usually want an ordinal, in the
            // interactive console usually a label.
            // will need a syntax for that, maybe ordinal(foo) or foo.ordinal

            UIParameterType ptype = s->parameter->type;
            if (ptype == TypeEnum) {
                result = juce::String(s->parameter->getEnumLabel(q.value));
            }
            else if (ptype == TypeBool) {
                result = (q.value == 1) ? "false" : "true";
            }
            else if (ptype == TypeStructure) {
                // hmm, the understand of LevelUI symbols that live in
                // UIConfig and LevelCore symbols that live in MobiusConfig
                // is in Supervisor right now
                // todo: Need to repackage this
                result = Supervisor::Instance->getParameterLabel(s, q.value);
            }
            else {
                // should only be here for TypeInt
                // unclear what String would do
                result = juce::String(q.value);
            }
        }
    }
    
    return result;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
