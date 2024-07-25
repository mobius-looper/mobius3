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
                      public Console::Listener,
                      public MslContext
{
  public:

    MobiusConsole(class ConsolePanel* panel);
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

    // MslContext
    juce::File mslGetRoot();
    class MobiusConfig* mslGetMobiusConfig() override;
    void mslDoAction(class UIAction* a) override;
    bool mslDoQuery(class Query* q) override;
    bool mslEcho(const char* msg) override;

  private:

    // handle to the global environment
    class MslEnvironment* scriptenv = nullptr;

    // scriptlet session we maintain
    class MslScriptletSession* session = nullptr;
    
    class ConsolePanel* panel = nullptr;
    BasicButtonRow commandButtons;
    Console console;

    void doLine(juce::String line);
    juce::String withoutCommand(juce::String line);
    void doHelp();
    
    void doLoad(juce::String line);
    void showLoad();

    void doParse(juce::String line);
    void doList();
    
    void doEval(juce::String line);
    void traceNode(class MslNode* node, int indent);

    void showErrors(juce::OwnedArray<class MslError>* errors);
};

