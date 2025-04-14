
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

#include "DialogTestTask.h"

DialogTestTask::DialogTestTask()
{
    type = Task::DialogTest;
}

void DialogTestTask::run()
{
    dialog.reset();

    //dialog.setTestBorders(juce::Colours::cyan);
    
    dialog.setTitle("What have we here now");
    dialog.addMessage("Are you sure?");
    dialog.addMessage("Really?");

    //dialog.addMessageGap();
    
    YanDialog::Message m;
    m.prefix = "Default folder:";
    m.prefixColor = juce::Colours::orange;
    m.prefixHeight = 20;
    m.message = "c:\\Users\\jeff\\dont\\look\\here";
    m.messageColor = juce::Colours::grey;
    m.messageHeight = 12;
    dialog.addMessage(m);
        
    dialog.addWarning("Something went wrong");
    dialog.addError("This is serioius");

    dialog.addField(&input);
    
    dialog.show(provider->getDialogParent());
}

void DialogTestTask::cancel()
{
    // todo: close any open dialogs
    finished = true;
}

void DialogTestTask::yanDialogClosed(YanDialog* d, int button)
{
    (void)d;
    (void)button;
    finished = true;
}
