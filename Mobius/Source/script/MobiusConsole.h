/**
 * An interactive console for the new MSL scripting lanauge.
 * Wrapped by ConsolePanel to give it life in the UI.
 */

#pragma once

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../ui/common/BasicButtonRow.h"

#include "Console.h"

class MobiusConsole : public juce::Component,
                      public juce::Button::Listener,
                      public Console::Listener
{
  public:

    MobiusConsole(class Supervisor* s, class ConsolePanel* panel);
    ~MobiusConsole();

    void showing();
    void hiding();
    void update();
    
    void resized() override;
    void paint(juce::Graphics& g) override;

    // Button::Listener
    void buttonClicked(juce::Button* b) override;

    // Console::Listener
    void consoleLine(juce::String line) override;
    void consoleEscape() override;

    // forwarded from Supervisor
    void mslPrint(const char* msg);

  private:

    class Supervisor* supervisor = nullptr;
    class MslEnvironment* scriptenv = nullptr;

    // scriptlet session we maintain
    class MslScriptlet* session = nullptr;
    int asyncSession = 0;
    
    class ConsolePanel* panel = nullptr;
    BasicButtonRow commandButtons;
    Console console;

    void doLine(juce::String line);
    juce::String withoutCommand(juce::String line);
    void doHelp();
    
    void doLoad(juce::String line);
    void showLoad();

    void doParse(juce::String line);
    void doPreproc(juce::String line);
    void doList();
    void doResume();
    void doStatus(juce::String line);
    void doResults(juce::String arg);
    
    void doEval(juce::String line);
    void traceNode(class MslNode* node, int indent);

    void showErrors(juce::OwnedArray<class MslError>* errors);
    void showErrors(class MslError* errors);
};

