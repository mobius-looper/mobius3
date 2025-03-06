/**
 * A slight extension of TreeView to get hooks into
 * being a DragAndDropTarget without having to fully subclass it.
 *
 * Feels like there should be an easier way.
 */

#pragma once

#include <JuceHeader.h>

class DropTreeView : public juce::TreeView
{
  public:

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void dropTreeViewDrop(DropTreeView* dtv, const juce::DragAndDropTarget::SourceDetails& details) = 0;
    };

    DropTreeView();

    void setListener(Listener* l);

    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDropped (const juce::DragAndDropTarget::SourceDetails&) override;

  private:

    Listener* listener = nullptr;

};


