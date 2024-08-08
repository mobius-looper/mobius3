/*
 * An extension of juce::Button to associate the visible
 * button with a Mobius Action.  These are arranged in a
 * configurable row by ActionButtons.
 */

#pragma once

#include <JuceHeader.h>

#include "../../model/UIAction.h"

class ActionButton : public juce::TextButton
{
  public:

    ActionButton(class ActionButtons* ab);
    ActionButton(class ActionButtons* ab, class DisplayButton* src);
    ActionButton(class ActionButtons* ab, class Symbol* src);
    ~ActionButton();

    int getPreferredWidth(int height);

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;
    
    class UIAction* getAction();

    void setDynamic(bool b) {
        dynamic = b;
    }

    bool isDynamic() {
        return dynamic;
    }

    void setColor(int c);
    int getColor() {
        return color;
    }

    void setTriggerId(int id);

    // called by ActionButtons as down/up transitions are detected
    void setDownTracker(bool b, bool rmb);
    bool isDownTracker();
    bool isDownRight();
    
  private:
    class ActionButtons* actionButtons = nullptr;

    UIAction action;
    bool dynamic = false;
    bool downTracker = false;
    bool downRight = false;
    int color = 0;
    
    void paintButton(juce::Graphics& g, juce::Colour background, juce::Colour text);
    
};


