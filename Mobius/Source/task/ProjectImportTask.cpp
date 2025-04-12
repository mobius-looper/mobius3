
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "../midi/MidiSequence.h"

#include "../mobius/MobiusInterface.h"
#include "../mobius/TrackContent.h"
#include "../mobius/Audio.h"
#include "../mobius/AudioFile.h"

#include "../Provider.h"

#include "Task.h"
#include "ProjectClerk.h"

#include "ProjectImportTask.h"

ProjectImportTask::ProjectImportTask()
{
    type = Task::ProjectImport;
    
    resultDialog.addButton("Ok");
}

void ProjectImportTask::run()
{
    finished = true;
}

void ProjectImportTask::cancel()
{
    // todo: close any open dialogs
    finished = true;
}

void ProjectImportTask::yanDialogClosed(YanDialog* d, int button)
{
    (void)d;
    (void)button;
}
