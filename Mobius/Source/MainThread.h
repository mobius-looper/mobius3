
#pragma once

#include <JuceHeader.h>
#include "util/Trace.h"

class MainThread : public juce::Thread, public TraceFlusher
{
  public:
    
    MainThread(class Supervisor* super);
    ~MainThread();

    void start();
    void stop();

    void run() override;

    // TraceListener
    void traceEvent() override;

  private:

    class Supervisor* supervisor;

    int counter = 0;
    
};

