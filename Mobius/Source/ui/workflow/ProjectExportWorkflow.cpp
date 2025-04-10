
#include <JuceHeader.h>

#include "ProjectExportWorkflow.h"

ProjectExportWorkflow::ProjectExportWorkflow()
{
}

ProjectExportWorkflow::~ProjectExportWorkflow()
{
}

void ProjectExportWorkflow::transition()
{
    if (step == StepBegin) {
    }
    else if (step == StepVerify) {
    }
    else if (step == StepResult) {
    }
    else {
        completeWorkflow();
    }
}
