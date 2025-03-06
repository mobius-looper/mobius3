
#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "DropTreeView.h"

DropTreeView::DropTreeView()
{
    setColour(juce::TreeView::backgroundColourId, juce::Colours::darkgrey.darker());
}

void DropTreeView::setListener(Listener* l)
{
    listener = l;
}

bool DropTreeView::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details)
{
    (void)details;
    return true;
}

void DropTreeView::itemDropped (const juce::DragAndDropTarget::SourceDetails& details)
{
    (void)details;

    Trace(2, "DropTreeView::itemDropped");
    if (listener != nullptr)
      listener->dropTreeViewDrop(this, details);
}

