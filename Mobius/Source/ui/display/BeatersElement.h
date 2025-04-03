/**
 * Status element to display the playback beats.
 */

#pragma once

#include <JuceHeader.h>

#include "../../Provider.h"

#include "StatusElement.h"

#define BEAT_DECAY 150

/**
 * Internal component maingained by Beaeters
 * Not a full StatusElement.
 * This really doesn't even need to be a Component and would
 * make mouse handling easier if it wasn't.
 */

class Beater : public juce::Component
{
  public:
    
    Beater() {
        setName("Beater");
    }

    ~Beater() {
    }
    
    // the sytem millisecond counter when this beater was turned on
    juce::uint32 startMsec = 0;
    // the amount of time to decay
    juce::uint32 decayMsec = 0;
    // are we on now?
    bool on = false;

    bool start(juce::uint32 now);
    bool tick(juce::uint32 now);

    void paintBeater(juce::Graphics& g);

    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
};


class BeatersElement : public StatusElement, public Provider::HighRefreshListener
{
  public:
    
    BeatersElement(class StatusArea* area);
    ~BeatersElement();

    void highRefresh(class PriorityState* s) override;
    
    int getPreferredWidth() override;
    int getPreferredHeight() override;

    void resized() override;
    void paint(juce::Graphics& g) override;
    
  private:

    Beater loopBeater;
    Beater cycleBeater;
    Beater subcycleBeater;
};

    
