/*
 * The default Mobius status display area.
 * This is what you see when you are not using configuration
 * popup editors.
 *
 * This will be inside MainWindow under the menu bar.
 * Eventually we can support alternative display styles within
 * MainWindow, under the control of DisplayManager
 *
 * At the top will be a row of configurable ActionButtons.
 * At the bottom will be a row of TrackStrips
 * In between is a configurable set of StatusElements.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "ActionButtons.h"
#include "StatusArea.h"
#include "TrackStrips.h"

class MobiusDisplay : public juce::Component
{
  public:

    MobiusDisplay(class MainWindow* main);
    ~MobiusDisplay();

    class Provider* getProvider();
    class MobiusView* getMobiusView();
    
    void configure();
    void captureConfiguration(class UIConfig* config);
    void update(class MobiusView* view);

    void resized() override;
    void paint (juce::Graphics& g) override;

    bool isIdentifyMode() {
        return statusArea.isIdentify();
    }

    void setIdentifyMode(bool b) {
        statusArea.setIdentify(b);
    }
    
  private:

    class MainWindow* mainWindow;

    ActionButtons buttons {this};
    StatusArea statusArea {this};
    TrackStrips strips {this};

};    

    
