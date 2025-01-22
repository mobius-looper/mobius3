
#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "YanDialog.h"

YanDialog::YanDialog()
{
}

YanDialog::YanDialog(Listener* l)
{
    listener = l;
}

YanDialog::~YanDialog()
{
}

void YanDialog::setListener(Listener* l)
{
    listener = l;
}

void YanDialog::setSerious(bool b)
{
    serious = b;
}

void YanDialog::setTitle(juce::String s)
{
    title = s;
}

void YanDialog::setMessage(juce::String s)
{
    message = s;
}

void YanDialog::addButton(juce::String text)
{
    buttons.add(text);
}

void YanDialog::show()
{
    showInternal();
}

void YanDialog::showInternal()
{
    // !!!!!!!!!!
    // like startSaveNew is it extremely dangerous to capture "this" because there is no assurance
    // that the user won't delete this tab while the browser is active
    // experiment with using a SafePointer or just passing some kind of unique
    // identifier and Supervisor, and letting Supervisor walk back down
    // to the ScriptEditorFile if it still exists
    
    // launch an async dialog box that calls the lambda when finished
    juce::MessageBoxOptions options = juce::MessageBoxOptions();

    options = options.withTitle(title);
    options = options.withMessage(message);

    if (serious)
      options = options.withIconType (juce::MessageBoxIconType::WarningIcon);

    for (auto button : buttons)
      options = options.withButton(button);
    
    juce::AlertWindow::showAsync (options, [this] (int button) {
        showFinished(button);
    });
}

void YanDialog::showFinished(int button)
{
    if (listener != nullptr)
      listener->yanDialogSelected(this, button);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
