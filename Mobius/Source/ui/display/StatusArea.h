/**
 * A component that displays a conigurable set of StatusElements
 * to display and control Mobius runtime state.
 *
 * Found at the in the center of MobiusDisplay.
 */

#pragma once

#include <JuceHeader.h>

#include "../../Supervisor.h"

#include "ModeElement.h"
#include "BeatersElement.h"
#include "LoopMeterElement.h"
#include "CounterElement.h"
#include "FloatingStripElement.h"
#include "ParametersElement.h"
#include "AudioMeterElement.h"
#include "LayerElement.h"
#include "AlertElement.h"
#include "MinorModesElement.h"
#include "TempoElement.h"
#include "LoopWindowElement.h"

class StatusArea : public juce::Component
{
  public:
    
    StatusArea(class MobiusDisplay*);
    ~StatusArea();

    class Supervisor* getSupervisor();
    class MobiusView* getMobiusView();
    
    void configure();
    void captureConfiguration(class UIConfig* config);
    
    void update(class MobiusView* view);
    
    // element callback to save location changes after dragging
    void saveLocation(StatusElement* e);

    void resized() override;
    void paint(juce::Graphics& g) override;

    // mouse handler when the cursor isn't over a StatusElement
    void mouseDown(const juce::MouseEvent& event) override;

    // element callback to force display of borders/labels
    bool isShowBorders() {
        return showBorders;
    }

    bool isIdentify() {
        return identify;
    }

    void setIdentify(bool b);
    
  private:

    class MobiusDisplay* display;
    juce::Array<StatusElement*> elements;
    bool showBorders = false;
    bool identify = false;
    
    ModeElement mode {this};
    BeatersElement beaters {this};
    LoopMeterElement meter {this};
    CounterElement counter {this};
    FloatingStripElement floater {this};
    ParametersElement parameters {this};
    AudioMeterElement audioMeter {this};
    LayerElement layers {this};
    AlertElement alerts {this};
    MinorModesElement minorModes {this};
    TempoElement tempo {this};
    LoopWindowElement loopWindow {this};
    
    void addElement(StatusElement* el);
    void addMissing(StatusElement* el);
    bool captureConfiguration(class DisplayLayout* layout, StatusElement* el);
    
};


    
