
#include <JuceHeader.h>

#include "NewConfigPanel.h"

NewConfigPanel::NewConfigPanel(ConfigEditor* argEditor, const char* titleText, int buttons, bool multi)
{
    setName("NewConfigPanel");
    editor = argEditor;

    setTitle(titleText);

    if (multi) {
        content.enableObjectSelector(this);
    }
    
    // convert the buttons spec into calls to BasePanel
    // to add buttons
    if (buttons & ButtonType::Save) {
    }
    if (buttons & ButtonType::Cancel) {
    }
        
    setContent(&content);
}

NewConfigPanel::~NewConfigPanel()
{
}

/**
 * Called by ConfigEditor each time one of the subclasses
 * is about to be shown.  Gives this a chance to do
 * potentially expensive initialization that we want
 * to avoid in the constructor, and save having to force the
 * subclasses to all call something to make it happen.
 */
void NewConfigPanel::prepare()
{
    if (!prepared) {
        // load the help catalog if it isn't already
        //helpArea.setCatalog(Supervisor::Instance->getHelpCatalog());
        prepared = true;
    }
}

//////////////////////////////////////////////////////////////////////
//
// ConfigPanelContent
//
//////////////////////////////////////////////////////////////////////

ConfigPanelContent::ConfigPanelContent()
{
    helpArea.setBackground(juce::Colours::black);
}

ConfigPanelContent::~ConfigPanelContent()
{
}

void ConfigPanelContent::setContent(juce::Component* c)
{
    content = c;
    addAndMakeVisible(c);
}

void ConfigPanelContent::enableObjectSelector(NewObjectSelector::Listener* l)
{
    objectSelectorEnabled = true;
    objectSelector.setListener(l);
    addAndMakeVisible(objectSelector);
}

void ConfigPanelContent::setHelpHeight(int h)
{
    helpHeight = h;
    if (helpHeight > 0)
      addAndMakeVisible(helpArea);
}        

void ConfigPanelContent::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    area.removeFromTop(4);
    
    if (objectSelectorEnabled) {
        objectSelector.setBounds(area.removeFromTop(objectSelector.getPreferredHeight()));
        area.removeFromTop(4);
    }

    if (helpHeight > 0) {
        helpArea.setBounds(area.removeFromBottom(helpHeight));
    }

    if (content != nullptr)
      content->setBounds(area);
}

//////////////////////////////////////////////////////////////////////
//
// ObjectSelector
//
//////////////////////////////////////////////////////////////////////

NewObjectSelector::NewObjectSelector()
{
    setName("ObjectSelector");

    addAndMakeVisible(combobox);
    combobox.addListener(this);
    combobox.setEditableText(true);
    
    addAndMakeVisible(newButton);
    newButton.addListener(this);
    
    addAndMakeVisible(deleteButton);
    deleteButton.addListener(this);

    // todo: have a copyButton, is this still needed?
    // getting implicit copy just by creating a new one
    // might be nice to have an "Init" button instead
}

NewObjectSelector::~NewObjectSelector()
{
}

int NewObjectSelector::getPreferredHeight()
{
    return 30;
}

void NewObjectSelector::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    // todo: calculate max width for object names?
    int comboWidth = 200;
    int comboHeight = 20;

    int centerLeft = (getWidth() - comboWidth) / 2;

    combobox.setBounds(centerLeft, area.getY(), comboWidth, comboHeight);

    newButton.setBounds(combobox.getX() + combobox.getWidth() + 4, area.getY(),
                        30, comboHeight);

    deleteButton.setBounds(newButton.getX() + newButton.getWidth() + 4, area.getY(),
                           50, comboHeight);

}

void NewObjectSelector::paint(juce::Graphics& g)
{
    (void)g;
}

/**
 * Now that we have an editable name, return the name.
 */
juce::String NewObjectSelector::getObjectName()
{
    return combobox.getText();
}

/**
 * Called by the ConfigPanel subclass to set the names
 * to display in the combobox.
 * When the combobox is changed we call the selectObject overload.
 * This also auto-selects the first name in the list.
 */
void NewObjectSelector::setObjectNames(juce::StringArray names)
{
    combobox.clear();
    // item ids must start from 1
    combobox.addItemList(names, 1);
    combobox.setSelectedId(1, juce::NotificationType::dontSendNotification);
    lastId = 1;
}

/**
 * Juce listener for the object management buttons.
 */
void NewObjectSelector::buttonClicked(juce::Button* b)
{
    if (listener != nullptr) {
        if (b == &newButton) {
            listener->objectSelectorNew();
        }
        else if (b == &deleteButton) {
            listener->objectSelectorDelete();
        }
        else if (b == &copyButton) {
            listener->objectSelectorCopy();
        }
    }
}

/**
 * Carefull here, some of the ComboBox methods use "index"
 * and some use "id".  Index is the zero based array index into
 * the item array, Id is the arbitrary number we can assigned
 * to the item at each index.
 *
 * This is how editable comboboxes seem to work.  If you edit
 * the text displayed in a combobox without using the item
 * selection menu, you get here with SelectedId == 0 and
 * getText returns the text that was entered. The items in
 * the menu do not change, and the checkboxes go away since what
 * is displayed in the text area doesn't match any of the items.
 *
 * If you type in a name that is the same as one of the existing items
 * sometimes it selects the item and sometimes it doesn't.  In my testing
 * I could get the first item selected by typing it's name but not the second.
 *
 * Tutorial on item id 0:
 * " You can use any integer as an item ID except zero. Zero has a special meaning. It
 * is used to indicate that none of the items are selected (either an item hasn't
 * been selected yet or the ComboBox object is displaying some other custom text)."
 *
 * So it kind of becomes a text entry field with a menu glued underneath to auto-fill
 * values.  You are NOT editing the text of an item.  To use this to implement item rename
 * you have to remember the id/index of the last item selected.  When you get
 * selectedId==0 compare the current text to the text of the last selected item and
 * if they are different treat as a rename.
 *
 * You can use escape to abandon the edit.  It appears the only reliable way to
 * have it select an existing item if you type in a matching name is to search and
 * select it in code, this doesn't seem to be automatic.
 * 
 */
void NewObjectSelector::comboBoxChanged(juce::ComboBox* combo)
{
    (void)combo;
    int id = combobox.getSelectedId();
    if (id == 0) {
        juce::String text = combobox.getText();
        juce::String itemText = combobox.getItemText(combobox.indexOfItemId(lastId));
        if (text != itemText) {
            // rename
            if (listener != nullptr)
              listener->objectSelectorRename(text);
            // change the text of the item too
            combobox.changeItemText(lastId, text);
        }
    }
    else {
        // ids are 1 based
        if (listener != nullptr)
          listener->objectSelectorSelect(id - 1);
        lastId = id;
    }
}

void NewObjectSelector::addObjectName(juce::String name)
{
    combobox.addItem(name, combobox.getNumItems() + 1);
}

/**
 * Note well: setSelectedId will by default result
 * in a change notification being sent to the listeners.
 * In this usage, the panel subclasses are managing their
 * own state, and just want to programatically move
 * the selected item. If you change this you need to make
 * sure that the subclass is prepared to immediately receive
 * a selectObject callback as if the user had interacted with
 * the combo box
 * 
 */ 
void NewObjectSelector::setSelectedObject(int ordinal)
{
    combobox.setSelectedId(ordinal + 1, juce::NotificationType::dontSendNotification);
}

// TODO: give the name label a listener to call renameObject

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
