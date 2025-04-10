/**
 * Base class for workflow processes.
 */

#pragma once

#include <JuceHeader.h>

class Workflow
{
  public:

    class Listener {
      public:
        virtual ~Lisener() {}
        virtual void workflowFinished(Workflow* wf);
    };
    
    virtual ~Workflow();

    void start(Provider* p, Listener* l);
    void complete();

    virtual void transition() = 0;
    
  private:

    Provider* provider = nullptr;
    Listener* listener = nullptr;
    
};
