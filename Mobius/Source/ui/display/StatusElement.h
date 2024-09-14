/**
 * Base class of a compone that displays a piece of Mobius
 * runtime state and optionally supports actions.
 */

#pragma once

#include <JuceHeader.h>

#include "StatusResizer.h"

class StatusElement : public juce::Component
{
  public:
    
    StatusElement(class StatusArea*, const char* id);
    ~StatusElement();

    virtual void configure();
    virtual void update(class MobiusView* view);
    virtual int getPreferredWidth();
    virtual int getPreferredHeight();
    
    virtual void resized() override;
    virtual void paint(juce::Graphics& g) override;
    bool isIdentify();
    
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& e) override;

    bool allowsResize() {
        return resizes;
    }
    
  protected:
    
    class StatusArea* statusArea;
    bool mouseEnterIdentify = false;
    // set by subclass if it wants to allow resizing
    bool resizes = false;

  private:

    //juce::ResizableBorderComponent resizer {this, nullptr};
    StatusResizer resizer {this};
    juce::ComponentDragger dragger;
    bool mouseEntered = false;
    
    // testing hack, StatusRotary now has one too
    bool dragging = false;

};


    
