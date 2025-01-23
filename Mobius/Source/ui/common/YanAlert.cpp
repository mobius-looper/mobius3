
#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "YanAlert.h"

YanAlert::YanAlert()
{
}

YanAlert::YanAlert(Listener* l)
{
    listener = l;
}

YanAlert::~YanAlert()
{
}

void YanAlert::setListener(Listener* l)
{
    listener = l;
}

void YanAlert::setSerious(bool b)
{
    serious = b;
}

void YanAlert::setTitle(juce::String s)
{
    title = s;
}

void YanAlert::setMessage(juce::String s)
{
    message = s;
}

void YanAlert::addButton(juce::String text)
{
    buttons.add(text);
}

//void YanAlert::addCustom(juce::Component* c)
//{
//    customs.add(c);
//}

void YanAlert::show()
{
    showInternal();
}

void YanAlert::showInternal()
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

    for (auto b : buttons)
      options = options.withButton(b);

    juce::AlertWindow::showAsync (options, [this] (int button) {
        showFinished(button);
    });
}

void YanAlert::showFinished(int button)
{
    // for some bizarre reason, button numbers seem come in out of order
    // If there are three buttons, the first is 1, second 2 and the last one is 0
    // maybe it's because I labeled it "Cancel"?
    int realButton = button - 1;
    if (realButton < 0) realButton = buttons.size() - 1;
    
    if (listener != nullptr)
      listener->yanAlertSelected(this, realButton);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
