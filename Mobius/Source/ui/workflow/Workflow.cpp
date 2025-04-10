
#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "Workflow.h"

Workflow::Wofkflow()
{
}

Workfflow::~Workflow()
{
}


void Workflow::start(Provider* p, Listener* l)
{
    provider = p;
    listener = l;

    transition();
}

void Workflow::complete()
{
    if (listener != nullptr)
      listener->workflowFinished(this);
}
