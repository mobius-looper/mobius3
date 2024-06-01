/**
 * Component that implements creation of a value with multiple strings
 * using a pair of ListBoxes with drag-and-drop of items.
 * The box on the left represents the desired value and the box on the right
 * represents the values available to be placed in it.
 *
 */

#pragma once

#include <JuceHeader.h>

//////////////////////////////////////////////////////////////////////
//
// HelpfulListBox
//
//////////////////////////////////////////////////////////////////////

/**
 * Need to break this out if we like it, others can use it too.
 */
class HelpListener
{
  public:
    virtual ~HelpListener() {}
    virtual void showHelp(juce::Component* component, juce::String name) = 0;
    virtual void hideHelp(juce::Component* component, juce::String name) = 0;
};

/**
 * Slight extension of ListBox that intercepts mouse entry
 * to display help text somewhere.
 *
 * todo: I think we don't need to subclass just to intercept mouse
 * events, shouldn't it be enough to register a MouseListener on
 * the base mouseEnter/mouseExit events?
 */
class HelpfulListBox : public juce::ListBox
{
  public:

    HelpfulListBox() {}
    ~HelpfulListBox() {}

    void setHelpListener(HelpListener* l, juce::String name) {
        helpListener = l;
        helpName = name;
    }
    
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    
  private:

    HelpListener* helpListener = nullptr;
    juce::String helpName;
    
};

//////////////////////////////////////////////////////////////////////
//
// StringArrayListBox
//
//////////////////////////////////////////////////////////////////////

/**
 * ListBox wrapper that Manages a StringArray and acts
 * as a drop target.  It is a wrapper rather than an extension
 * to give the ListBox a small inset where a border can be drawn
 * when a drop is over it.
 *
 * Juce tidbit: I wanted this to behave like the drag-and-drop
 * demo drawing a border around the component when a drop hovers
 * over it, but you can't just override ListBox::paint.  It seems
 * that the when the childre completly cover the area of the parent,
 * the parent's paint method will not be called.
 */
class StringArrayListBox : public juce::Component,
                           public juce::ListBoxModel,
                           public juce::DragAndDropTarget
{
  public:

    /**
     * Listener to inform something (MultiSelectDrag) that new values
     * have been dropped in.
     */
    class Listener {
      public:
        virtual ~Listener() {};
        virtual void valuesReceived(StringArrayListBox* box, juce::StringArray& values) = 0;
    };

    StringArrayListBox();
    ~StringArrayListBox();

    void setHelpListener(HelpListener* l, juce::String helpName) {
        box.setHelpListener(l, helpName);
    }
    juce::Component* getHelpSource() {
        return &box;
    }

    void setValue(juce::StringArray& value);
    juce::StringArray getValue();

    void setListener(Listener* l);
    void setSorted(bool b);
    
    void clear();
    void remove(juce::StringArray& values);
    
    void resized() override;
    void paint(juce::Graphics&) override;

    void setMouseSelect(bool b) {
        (void)b;
        box.setMouseMoveSelectsRows(true);
    }

    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g,
                           int width, int height, bool rowIsSelected) override;
    juce::var getDragSourceDescription (const juce::SparseSet<int>& selectedRows) override;
    //juce::MouseCursor getMouseCursorForRow(int row) override;
    //juce::String getTooltipForRow(int row) override;
    
    // DragAndDropTarget
    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails&) override;
    void itemDragEnter (const juce::DragAndDropTarget::SourceDetails&) override;
    void itemDragMove (const juce::DragAndDropTarget::SourceDetails&) override;
    void itemDragExit (const juce::DragAndDropTarget::SourceDetails&) override;
    void itemDropped (const juce::DragAndDropTarget::SourceDetails&) override;

  private:

    // juce::ListBox box;
    HelpfulListBox box;
    juce::StringArray strings;
    bool sorted = false;
    Listener* listener = nullptr;

    bool targetActive = false;
    bool moveActive = false;
    int lastInsertIndex = -1;

    int getDropRow(const juce::DragAndDropTarget::SourceDetails& details);
};

//////////////////////////////////////////////////////////////////////
//
// MultiSelectDrag
//
//////////////////////////////////////////////////////////////////////

class MultiSelectDrag : public juce::Component, public juce::DragAndDropContainer,
                        public StringArrayListBox::Listener,
                        public HelpListener
{
  public:

    MultiSelectDrag();
    ~MultiSelectDrag();

    void setLabel(juce::String s) {
        label = s;
    }

    void setHelpArea(class HelpArea* area, juce::String prefix);
    
    void clear();
    void setValue(juce::StringArray strings, juce::StringArray allowed);
    juce::StringArray getValue();

    int getPreferredHeight();
    void resized() override;
    void paint(juce::Graphics& g) override;

    // StringArrayListBox::Listener
    void valuesReceived(StringArrayListBox* box, juce::StringArray& values) override;

    // HelpListener redirect from internal components
    void showHelp(juce::Component* c, juce::String name) override;
    void hideHelp(juce::Component* c, juce::String name) override;
    // pick up when mouse is over our label
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    
  private:

    juce::String label;
    StringArrayListBox valueBox;
    StringArrayListBox availableBox;

    class HelpArea* helpArea = nullptr;
    juce::String helpPrefix;
};
