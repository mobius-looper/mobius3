
#include <JuceHeader.h>

#include "TargetSelectorWrapper.h"
#include "BindingTargetSelector.h"


TargetSelectorWrapper::TargetSelectorWrapper(Supervisor* s) : oldSelector(s), newSelector(s)
{
    if (useNew) {
        addAndMakeVisible(newSelector);
    }
    else {
        oldSelector.setListener(this);
        addAndMakeVisible(oldSelector);
    }
}

TargetSelectorWrapper::~TargetSelectorWrapper()
{
}

void TargetSelectorWrapper::setListener(Listener* l)
{
    listener = l;
}

void TargetSelectorWrapper::load()
{
    if (useNew)
      newSelector.load();
    else
      oldSelector.load();
}

void TargetSelectorWrapper::reset()
{
    if (useNew)
      newSelector.reset();
    else
      oldSelector.reset();
}

void TargetSelectorWrapper::select(class Binding* b)
{
    if (useNew)
      newSelector.select(b);
    else
      oldSelector.select(b);
}

void TargetSelectorWrapper::capture(class Binding* b)
{
    if (useNew)
      newSelector.capture(b);
    else
      oldSelector.capture(b);
}

bool TargetSelectorWrapper::isTargetSelected()
{
    if (useNew) return newSelector.isTargetSelected();
    return oldSelector.isTargetSelected();
}

void TargetSelectorWrapper::resized()
{
    if (useNew)
      newSelector.setBounds(getLocalBounds());
    else
      oldSelector.setBounds(getLocalBounds());
}
 
void TargetSelectorWrapper::bindingTargetClicked(BindingTargetSelector* bts)
{
    (void)bts;
    if (listener != nullptr)
      listener->bindingTargetClicked();
}
