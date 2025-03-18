/**
 * An interactive console for the new MSL scripting lanauge.
 * Wrapped by ConsolePanel to give it life in the UI.
 */

#pragma once

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../common/BasicButtonRow.h"

#include "Console.h"

class MclConsole : public juce::Component,
                      public juce::Button::Listener,
                      public Console::Listener
{
  public:

    MclConsole(class Supervisor* s, class MclPanel* panel);
    ~MclConsole();

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

  private:

    class Supervisor* supervisor = nullptr;
    class MclSession* mclSession = nullptr;

    class MclPanel* panel = nullptr;
    BasicButtonRow commandButtons;
    Console console;

    void doLine(juce::String line);
    juce::String withoutCommand(juce::String line);
    void doHelp();
    void doEval(juce::String line);
    
};

