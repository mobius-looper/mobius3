/**
 * Popup component within ScriptLibraryTable to show file details.
 */

#pragma once

#include <JuceHeader.h>

#include "../common/BasicButtonRow.h"
#include "../ScriptDetails.h"

class ScriptFileDetails : public juce::Component, public juce::Button::Listener
{
  public:

    ScriptFileDetails();
    ~ScriptFileDetails();

    void resized() override;
    void paint(juce::Graphics& g) override;
    void buttonClicked(juce::Button* b) override;

    void show(class ScriptRegistry::File* file);
    void hide();

    // Drag and Resize
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& e) override;
    
  private:

    ScriptDetails details;
    
    BasicButtonRow closeButtons;
    juce::TextButton okButton {"Close"};
    bool shownOnce = false;
    
    juce::ComponentDragger dragger;
    juce::ComponentBoundsConstrainer dragConstrainer;
    bool dragging = false;
    
};
