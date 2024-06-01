
#pragma once

#include <JuceHeader.h>

class DiagnosticWindowMain : public juce::Component
{
  public:
    DiagnosticWindowMain();
    ~DiagnosticWindowMain();
};

class DiagnosticWindow : public juce::DocumentWindow
{
  public:
    
    DiagnosticWindow();
    ~DiagnosticWindow();

    void closeButtonPressed();

    static DiagnosticWindow* launch();
    
  private:

};
