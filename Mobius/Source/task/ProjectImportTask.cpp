
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
    resultDialog.setHeight(300);
    resultDialog.setTitleHeight(30);
    resultDialog.setMessageHeight(20);
    resultDialog.setTitleGap(20);
    resultDialog.setSectionHeight(20);
    resultDialog.setErrorHeight(20);
    resultDialog.setButtonGap(20);
    resultDialog.setTitle("What have we here now");
    resultDialog.setTitleColor(juce::Colours::pink);
    resultDialog.addMessage("Are you sure?");
    resultDialog.addMessage("Really?");

    resultDialog.addMessageGap();
    
    YanDialog::Message m;
    m.prefix = "Default folder:";
    m.prefixColor = juce::Colours::orange;
    m.message = "c:\\Users\\jeff\\dont\\look\\here";
    m.messageColor = juce::Colours::grey;
    m.messageHeight = 12;
    resultDialog.addMessage(m);
        
    resultDialog.addWarning("Something went wrong");
    resultDialog.addError("This is serioius");
    resultDialog.show(provider->getDialogParent());
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
    finished = true;
}
