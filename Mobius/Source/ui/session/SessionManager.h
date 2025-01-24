/**
 * Displays information about all of the available Sessions
 * and provides ways to manage them.  Contained within SessionManagerPanel
 */

#pragma once

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../common/BasicButtonRow.h"

#include "SessionManagerTable.h"

class SessionManager : public juce::Component
{
  public:

    SessionManager(class Supervisor* s, class SessionManagerPanel* panel);
    ~SessionManager();

    void showing();
    void hiding();
    void update();
    
    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    class Supervisor* supervisor = nullptr;
    class SessionManagerPanel* panel = nullptr;

    SessionManagerTable sessions;

};

