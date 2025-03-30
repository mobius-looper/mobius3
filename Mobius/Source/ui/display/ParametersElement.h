/**
 * Status element to display a configued set of Parameter values
 * and allow temporary editing.
 */

#pragma once

#include <JuceHeader.h>

#include "../../Provider.h"
#include "StatusElement.h"

class ParametersElement : public StatusElement,
                          public Provider::ActionListener
{
  public:

    /**
     * Small helper structure to keep the parameter symbol we're displaying
     * along with it's last displayed value.  Reduces flicker when
     * reconfiguring the parameter list so we can keep the last value
     * if the same parameter is found in the old and new lists.
     */
    class ParameterState {
      public:
        class Symbol* symbol = nullptr;
        int value = 0;
        bool looksGood = false;
        ParameterState() {}
        ~ParameterState() {}
    };
    
    ParametersElement(class StatusArea* area);
    ~ParametersElement();

    void configure() override;
    void update(class MobiusView* view) override;
    
    int getPreferredWidth() override;
    int getPreferredHeight() override;

    void resized() override;
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    bool doAction(class UIAction* action) override;
    
  private:

    juce::OwnedArray<ParameterState> parameters;
    int maxNameWidth = 0;
    int maxValueWidth = 0;
    int cursor = 0;

    bool valueDrag = false;
    int valueDragStart = 0;
    int valueDragMin = 0;
    int valueDragMax = 0;
    
    juce::String getDisplayName(Symbol* s);
    int getMax(ParameterState* ps);
    int getMin(ParameterState* ps);
    bool isUnresolved(ParameterState* ps);
    juce::String formatFloat(int value);
    
};

    
