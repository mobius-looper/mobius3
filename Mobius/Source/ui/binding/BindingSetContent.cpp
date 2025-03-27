
#include <JuceHeader.h>

#include "BindingTable.h"
#include "BindingSetContent.h"

BindingSetContent::BindingSetContent()
{
    addAndMakeVisible(table);
}

void BindingSetContent::load(class BindingEditor* ed, class BindingSet* set)
{
    table.load(ed, set);
}

void BindingSetContent::cancel()
{
    table.cancel();
}

void BindingSetContent::resized()
{
    table.setBounds(getLocalBounds());
}
