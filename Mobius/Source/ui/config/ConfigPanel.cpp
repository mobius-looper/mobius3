/**
 * Base class for all configuration and information popup dialogs.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../Supervisor.h"
#include "../common/HelpArea.h"

#include "ConfigEditor.h"
#include "ConfigPanel.h"

// this can't scroll so make it tall enough for all the possible help the
// subclasses need
const int ConfigPanelHelpHeight = 60;

ConfigPanel::ConfigPanel(ConfigEditor* argEditor, const char* titleText, int buttons, bool multi)
    : objectSelector{this}, header{titleText}, footer{this,buttons}
{
    setName("ConfigPanel");
    editor = argEditor;

    addAndMakeVisible(header);
    addAndMakeVisible(footer);
    addAndMakeVisible(helpArea);
    helpArea.setBackground(juce::Colours::black);
    addAndMakeVisible(content);

    if (multi) {
        hasObjectSelector = true;
        addAndMakeVisible(objectSelector);
    }
    
    // size has to be deferred to the subclass after it has finished rendering

    helpHeight = ConfigPanelHelpHeight;

    // pass back mouse events from the header so we can drag
    header.addMouseListener(this, true);
}

ConfigPanel::~ConfigPanel()
{
}

void ConfigPanel::setHelpHeight(int h)
{
    helpHeight = h;
}

/**
 * New/better way to set the editing component in the center.
 * This replaces the original content container which will
 * be removed when everything starts ddoing it this way.
 */
void ConfigPanel::setMainContent(juce::Component* c)
{
    main = c;
    addAndMakeVisible(main);
    removeChildComponent(&content);
}

/**
 * Called by ConfigEditor each time one of the subclasses
 * is about to be shown.  Gives this a chance to do
 * potentially expensive initialization that we want
 * to avoid in the constructor, and save having to force the
 * subclasses to all call something to make it happen.
 */
void ConfigPanel::prepare()
{
    if (!prepared) {
        // load the help catalog if it isn't already
        helpArea.setCatalog(Supervisor::Instance->getHelpCatalog());
        prepared = true;
    }
}

/**
 * Called by the footer when a button is clicked
 *
 * !! I don't like the way the "loaded" flag is used to
 * mean both "I've done my complex initialization" and
 * "I no longer want to be visible".  Revisit this and
 * make it more obvious why subclasses MUST ad loaded=false
 * in save/cancel.
 */
void ConfigPanel::footerButtonClicked(ConfigPanelButton button)
{
    switch (button) {
        case (ConfigPanelButton::Ok):
        case (ConfigPanelButton::Save): {
            save();
        }
        break;
        case (ConfigPanelButton::Cancel): {
            cancel();
        }
        break;
        case (ConfigPanelButton::Revert): {
            revertObject();
        }
        break;
    }

    // ConfigEditor will decide whether to show
    // another editor panel if one has unsaved changes
    if (editor != nullptr)
      editor->close(this);
}

/**
 * Calculate the preferred width for the configued components.
 * MainComponent will use this to set our size, then we adjust downward.
 */
#if 0
int ConfigPanel::getPreferredHeight()
{
    return 0;
}
#endif

/**
 * TODO: MainComponent will give us it's maximum size.
 * We wander through the configured child components asking for their
 * preferred sizes and shrink down if possible.
 */
void ConfigPanel::resized()
{
    juce::Rectangle area = getLocalBounds();
    // surrounding border
    area = area.reduced(5);
    
    header.setBounds(area.removeFromTop(header.getPreferredHeight()));

    // leave a little space under the header
    area.removeFromTop(4);
    
    if (hasObjectSelector) {
        objectSelector.setBounds(area.removeFromTop(objectSelector.getPreferredHeight()));
        area.removeFromTop(4);
    }

    footer.setBounds(area.removeFromBottom(footer.getPreferredHeight()));

    helpArea.setBounds(area.removeFromBottom(helpHeight));
    
    // new way
    if (main != nullptr)
      main->setBounds(area);
    else
      content.setBounds(area);
}

/**
 * ConfigPanels are not at the moment resizeable, but they
 * can auto-center within the parent.
 */
void ConfigPanel::center()
{
    int pwidth = getParentWidth();
    int pheight = getParentHeight();
    
    int mywidth = getWidth();
    int myheight = getHeight();
    
    if (mywidth > pwidth) mywidth = pwidth;
    if (myheight > pheight) myheight = pheight;

    int left = (pwidth - mywidth) / 2;
    int top = (pheight - myheight) / 2;
    
    setTopLeftPosition(left, top);
}

void ConfigPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    g.setColour(juce::Colours::white);
    g.drawRect(getLocalBounds(), 4);
}

void ConfigPanel::mouseDown(const juce::MouseEvent& e)
{
    dragger.startDraggingComponent(this, e);
    // the first argu is "minimumWhenOffTheTop" set
    // this to the full height and it won't allow dragging the
    // top out of boundsa
    dragConstrainer.setMinimumOnscreenAmounts(getHeight(), 100, 100, 100);
}

void ConfigPanel::mouseDrag(const juce::MouseEvent& e)
{
    dragger.dragComponent(this, e, &dragConstrainer);
}

//////////////////////////////////////////////////////////////////////
//
// Header
//
//////////////////////////////////////////////////////////////////////

ConfigPanelHeader::ConfigPanelHeader(const char* titleText)
{
    setName("ConfigPanelHeader");
    addAndMakeVisible (titleLabel);
    titleLabel.setFont (juce::Font (16.0f, juce::Font::bold));
    titleLabel.setText (titleText, juce::dontSendNotification);
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType (juce::Justification::centred);
}


ConfigPanelHeader::~ConfigPanelHeader()
{
}

int ConfigPanelHeader::getPreferredHeight()
{
    // todo: ask the title font
    return 30;
}

void ConfigPanelHeader::resized()
{
    // let it fill the entire area
    titleLabel.setBounds(getLocalBounds());
}

void ConfigPanelHeader::paint(juce::Graphics& g)
{
    // give it an obvious background
    // need to work out  borders
    g.fillAll (juce::Colours::blue);
}

//////////////////////////////////////////////////////////////////////
//
// Footer
//
//////////////////////////////////////////////////////////////////////

ConfigPanelFooter::ConfigPanelFooter(ConfigPanel* parent, int buttons)
{
    setName("ConfigPanelFooter");
    parentPanel = parent;
    buttonList = buttons;

    if (buttons & ConfigPanelButton::Ok) {
        addButton(&okButton, "Ok");
    }

    if (buttons & ConfigPanelButton::Save) {
        addButton(&saveButton, "Save");
    }
    
    if (buttons & ConfigPanelButton::Revert) {
        addButton(&revertButton, "Revert");
    }
    
    if (buttons & ConfigPanelButton::Cancel) {
        addButton(&cancelButton, "Cancel");
    }

}

ConfigPanelFooter::~ConfigPanelFooter()
{
}

void ConfigPanelFooter::addButton(juce::TextButton* button, const char* text)
{
    addAndMakeVisible(button);
    button->setButtonText(text);
    button->addListener(this);
}

/**
 * This effectively determines the height of the save/cancel buttons at the bottom
 * Started with 36 which made them pretty chonky.
 */
int ConfigPanelFooter::getPreferredHeight()
{
    // todo: more control over the internal button sizes
    return 24;
}

void ConfigPanelFooter::resized()
{
    auto area = getLocalBounds();
    const int buttonWidth = 100;

    // seems like centering should be easier...
    // don't really need to deal with having both Ok and Save there
    // will only ever be two
    int numButtons = 0;
    if (buttonList & ConfigPanelButton::Ok) numButtons++;
    if (buttonList & ConfigPanelButton::Save) numButtons++;
    if (buttonList & ConfigPanelButton::Cancel) numButtons++;
    if (buttonList & ConfigPanelButton::Revert) numButtons++;

    int totalWidth = area.getWidth();
    int buttonsWidth = buttonWidth * numButtons;
    int leftOffset = (totalWidth / 2) - (buttonsWidth / 2);
    area.removeFromLeft(leftOffset);
    
    if (buttonList & ConfigPanelButton::Ok) {
        okButton.setBounds(area.removeFromLeft(buttonWidth));
    }
    
    if (buttonList & ConfigPanelButton::Save) {
        saveButton.setBounds(area.removeFromLeft(buttonWidth));
    }

    if (buttonList & ConfigPanelButton::Revert) {
        revertButton.setBounds(area.removeFromLeft(buttonWidth));
    }
    
    if (buttonList & ConfigPanelButton::Cancel) {
        cancelButton.setBounds(area.removeFromLeft(buttonWidth));
    }
}

void ConfigPanelFooter::paint(juce::Graphics& g)
{
    // buttons will draw themselves in whatever the default color is
    g.fillAll (juce::Colours::black);
}

/**
 * juce::Button::Listener
 * Forward to the parent
 */
void ConfigPanelFooter::buttonClicked(juce::Button* b)
{
    // find a better way to do this, maybe subclassing Button
    if (b == &okButton) {
        parentPanel->footerButtonClicked(ConfigPanelButton::Ok);
    }
    else if (b == &saveButton) {
        parentPanel->footerButtonClicked(ConfigPanelButton::Save);
    }
    else if (b == &cancelButton) {
        parentPanel->footerButtonClicked(ConfigPanelButton::Cancel);
    }
    else if (b == &revertButton) {
        parentPanel->footerButtonClicked(ConfigPanelButton::Revert);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Content
//
// Nothing really to do here.  If all subclasses just have a single
// component could do away with this, but it is a nice spot to leave
// the available area.
//
//////////////////////////////////////////////////////////////////////

ContentPanel::ContentPanel()
{
    setName("ContentPanel");
}

ContentPanel::~ContentPanel()
{
}

void ContentPanel::resized()
{
    // assume subclass added a single child
    Component* child = getChildComponent(0);
    if (child != nullptr)
      child->setSize(getWidth(), getHeight());
}

void ContentPanel::paint(juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

//////////////////////////////////////////////////////////////////////
//
// ObjectSelector
//
//////////////////////////////////////////////////////////////////////

ObjectSelector::ObjectSelector(ConfigPanel* parent)
{
    setName("ObjectSelector");
    parentPanel = parent;

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

ObjectSelector::~ObjectSelector()
{
}

int ObjectSelector::getPreferredHeight()
{
    return 30;
}

void ObjectSelector::resized()
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

void ObjectSelector::paint(juce::Graphics& g)
{
    (void)g;
}

/**
 * Now that we have an editable name, return the name.
 */
juce::String ObjectSelector::getObjectName()
{
    return combobox.getText();
}

/**
 * Called by the ConfigPanel subclass to set the names
 * to display in the combobox.
 * When the combobox is changed we call the selectObject overload.
 * This also auto-selects the first name in the list.
 */
void ObjectSelector::setObjectNames(juce::StringArray names)
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
void ObjectSelector::buttonClicked(juce::Button* b)
{
    if (b == &newButton) {
        parentPanel->newObject();
    }
    else if (b == &deleteButton) {
        parentPanel->deleteObject();
    }
    else if (b == &copyButton) {
        parentPanel->copyObject();
    }

    // decided to put the revert button in the footer rather than up here
    /*
    else if (b == &revertButton) {
        parentPanel->revertObject();
    }
    */
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
void ObjectSelector::comboBoxChanged(juce::ComboBox* combo)
{
    (void)combo;
    int id = combobox.getSelectedId();
    if (id == 0) {
        juce::String text = combobox.getText();
        juce::String itemText = combobox.getItemText(combobox.indexOfItemId(lastId));
        if (text != itemText) {
            // rename
            parentPanel->renameObject(text);
            // change the text of the item too
            combobox.changeItemText(lastId, text);
        }
    }
    else {
        // ids are 1 based
        parentPanel->selectObject(id - 1);
        lastId = id;
    }
}

void ObjectSelector::addObjectName(juce::String name)
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
void ObjectSelector::setSelectedObject(int ordinal)
{
    combobox.setSelectedId(ordinal + 1, juce::NotificationType::dontSendNotification);
}

// TODO: give the name label a listener to call renameObject

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
