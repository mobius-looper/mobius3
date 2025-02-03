
#include <JuceHeader.h>

#include "../../util/Trace.h"


#include "YanPopup.h"

YanPopup::YanPopup()
{
}

YanPopup::YanPopup(Listener* l)
{
    listener = l;
}

YanPopup::~YanPopup()
{
}

void YanPopup::setListener(Listener* l)
{
    listener = l;
}

void YanPopup::clear()
{
    menu.clear();
}

void YanPopup::add(juce::String text, int id, bool ticked)
{
    // other things you can do
    // setEnabled, setTicked, setAction, setImage
    juce::PopupMenu::Item item (text);
    item.setID(id);
    item.setTicked(ticked);
    menu.addItem(item);
}

void YanPopup::addDivider()
{
    // also too: addSectionHeader
    menu.addSeparator();
}

/**
 * Doc example:
 *
 * PopupMenu menu;
 * ...
 * menu.showMenu (PopupMenu::Options().withMinimumWidth (100)
 *                                    .withMaximumNumColumns (3)
 *                                    .withTargetComponent (myComp));
 *
 * By default this is showing with the mouse in the upper left corner of the
 * menu, which causes the first item to be selected if you immmediately release
 * the button.  Would rather this show to the right with no selection.
 */
void YanPopup::show()
{
    //Trace(2, "YanPopup::show");
    
    // second arg is a ModalComponentManager::Callback
    // a lambda that gets called when a selection is made
    
    //juce::PopupMenu::Options options;
    //menu.showMenuAsync(options, [this] (int result) {popupSelection(result);});

    // this didn't work, play around with withTargetScreenArea someday
    menu.showMenuAsync(juce::PopupMenu::Options().withMousePosition(),
                       [this] (int result) {popupSelection(result);});
}

void YanPopup::popupSelection(int result)
{
    //Trace(2, "Popup menu selected %d\n", result);
    if (listener != nullptr)
      listener->yanPopupSelected(this, result);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
