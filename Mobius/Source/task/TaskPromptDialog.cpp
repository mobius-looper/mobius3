
#include <JuceHeader.h>

#include "TaskPromptDialog.h"

TaskPromptDialog::TaskPromptDialog(YanDialog::Listener* l) : YanDialog(l)
{
    setErrorHeight(20);
    setWarningHeight(16);
}

TaskPromptDialog::~TaskPromptDialog()
{
}
