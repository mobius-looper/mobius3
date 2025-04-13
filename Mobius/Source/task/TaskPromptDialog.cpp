
#include <JuceHeader.h>

#include "TaskPromptDialog.h"

TaskPromptDialog::TaskPromptDialog(YanDialog::Listener* l) : YanDialog(l)
{
    setHeight(300);
    setTitleHeight(30);
    setMessageHeight(20);
    setTitleGap(20);
    setSectionHeight(20);
    setErrorHeight(20);
    setButtonGap(20);
}

TaskPromptDialog::~TaskPromptDialog()
{
}
