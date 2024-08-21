/**
 * Component that implements creation of a value with multiple strings
 * using a pair of ListBoxes with drag-and-drop of items.
 * The box on the left represents the desired value and the box on the right
 * represents the values available to be placed in it.
 *
 * Ugh, intercepting mouseEnter for ListBox to show help text is annoying
 * because the ListBox itself has subcomponents and the lowest subcomponent
 * is what gets the MouseEvent.  In this case the RowComponent.  THAT is what you
 * would need to subclass in order to do the help listener, or provide item specific
 * help.  Here is the JuceUtil dump:
 * 
 *   class StringArrayListBox: 225 14 225 104
 *     class HelpfulListBox: 1 1 223 102
 *       class juce::ListBox::ListViewport: 0 0 223 102
 *         class juce::Viewport::AccessibilityIgnoredComponent: 0 0 215 102
 *           struct `public: __cdecl juce::ListBox::ListViewport::ListViewport(class juce::ListBox & __ptr64) __ptr64'::`2'::IgnoredComponent: 0 0 215 2508
 *             class juce::ListBox::RowComponent: 0 0 215 22
 *
 * There doesn't seem to be a way to pass down a RowComponent during construction.
 * There is ListBox::getComponentForRowNumber which might dig it out and we could
 * mess with.
 *
 * From the ListBoxModel there is refreshComponentForRow which "is used to create or update a
 * custom component to go in a row of the list.  That sounds promising.
 *
 * A simpler hook may be ListBoxModel::getMouseCursorForRow which
 * "You can override this to return a custom mouse cursor for each row".
 * We don't need to change the cursor, but we could use that to change the help.
 *
 * ugh, I'm about ready to toss the towel.  getMouseCursorForRow doesn't work because
 * it is called once for all rows when you click on one, not as the mouse hovers over one.
 * A custom component is proably the only way, but I now notice "tool tips" which
 * looks very much like what I want, except I don't like how there has to be a single
 * global ToolTipClient.  It's probably a popup window kind of thing like traditional tool
 * tips, not the "always there" tooltip like Ableton has and is what I'm going for.
 *
 * Forum chatter on custom row components:
 *  https://forum.juce.com/t/question-about-custom-listbox-components/19397/3
 * Also this tutorial on TableListBox
 *   https://docs.juce.com/master/tutorial_table_list_box.html
 *
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../common/HelpArea.h"
#include "../JuceUtil.h"

#include "MultiSelectDrag.h"

//////////////////////////////////////////////////////////////////////
//
// MultiSelectDrag
//
//////////////////////////////////////////////////////////////////////

const int MultiSelectDragLabelHeight = 14;

MultiSelectDrag::MultiSelectDrag()
{
    // the current list is ordered for the track strips
    // and instant parameters
    valueBox.setListener(this);
    // why set this here but not the other?
    // valueBox.setMouseSelect(true);
    addAndMakeVisible(valueBox);

    // the availables are always sorted
    availableBox.setListener(this);
    availableBox.setSorted(true);
    addAndMakeVisible(availableBox);
}

MultiSelectDrag::~MultiSelectDrag()
{
}

void MultiSelectDrag::setHelpArea(HelpArea* area, juce::String prefix)
{
    helpArea = area;
    helpPrefix = prefix;

    // give each of the embedded StringListBox/HelpfulListBox
    // a helpName and tell them to redirect mouse movements to us
    valueBox.setHelpListener(this, prefix + "Current");
    availableBox.setHelpListener(this, prefix + "Available");
}

void MultiSelectDrag::clear()
{
    valueBox.clear();
    availableBox.clear();
}

juce::StringArray MultiSelectDrag::getValue()
{
    return valueBox.getValue();
}

// todo: be smarter here if we need it at all
int MultiSelectDrag::getPreferredHeight()
{
    return 100;
}

/**
 * "current" has the starting value, "allowed" has the full set
 * of allowed values.  To build the available list, remove those that
 * are already in the current value.
 * 
 * todo: I supose we verify that the current value items are all
 * in the allowed list but that would be a data error the caller
 * should have dealt with by now.
 */
void MultiSelectDrag::setValue(juce::StringArray current, juce::StringArray allowed)
{
    valueBox.setValue(current);

    // didn't see an operator for this
    for (auto s : current)
      allowed.removeString(s);

    availableBox.setValue(allowed);
}

/**
 * Set the complete set of allowed values.
 * Newer interface for multiselects that are reused with different values.
 */
void MultiSelectDrag::setAllowed(juce::StringArray allowed)
{
    allAllowed = allowed;
}

void MultiSelectDrag::setValue(juce::StringArray current)
{
    valueBox.setValue(current);

    juce::StringArray allowed = allAllowed;
    for (auto s : current)
      allowed.removeString(s);
    
    availableBox.setValue(allowed);
}

/**
 * Put them side by side and leave a gap in between
 */
void MultiSelectDrag::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    if (label.length() > 0)
      area.removeFromTop(MultiSelectDragLabelHeight);
    
    int gap = 20;
    int boxWidth = (area.getWidth() / 2) - gap;
    valueBox.setBounds(area.removeFromLeft(boxWidth));
    area.removeFromLeft(gap);
    availableBox.setBounds(area);
}

/**
 * Need some labeling and help text that explains what the
 * user is supposed to do...
 */
void MultiSelectDrag::paint(juce::Graphics& g)
{
    if (label.length() > 0) {
        juce::Font font = juce::Font(juce::FontOptions((float)MultiSelectDragLabelHeight));
        g.setFont(font);
        g.setColour (juce::Colours::white);
        // The ListBox has a 1 pixel border for the drop highlight
        // indent the label a little
        int labelLeft = 2;
        g.drawText(label, labelLeft, 0, getWidth(), MultiSelectDragLabelHeight, juce::Justification::left);
    }
}

/**
 * Called by one of our StringArrayListBoxes, when
 * values have been dragged from one to the other.
 * The target box has already added the values to itself,
 * here we remove them from the source box.
 */
void MultiSelectDrag::valuesReceived(StringArrayListBox* box, juce::StringArray& values)
{
    if (box == &valueBox)
      availableBox.remove(values);
    else
      valueBox.remove(values);
}

/**
 * Help handlers sent up from the HelpfulListBox under
 * our StringListbox.  The helpName given to the component
 * can be used as the catalog key.
 *
 * This turns out not be useful because ListBox has several
 * layers of component structure down to the RowComponent which
 * is what actually receives the mouse events.  We WILL get this
 * callback if you hover space in the ListBox where there is no row,
 * but most of my boxes are full.
 */
void MultiSelectDrag::showHelp(juce::Component* c, juce::String key)
{
    (void)c;
    //Trace(2, "MultiSelectDrag::showHelp %s\n", key.toUTF8());
    if (helpArea != nullptr)
      helpArea->showHelp(key);
}

void MultiSelectDrag::hideHelp(juce::Component* c, juce::String key)
{
    (void)c;
    //Trace(2, "MultiSelectDrag::hideHelp %s\n", key.toUTF8());
    if (helpArea != nullptr) {
        helpArea->clear();
    }
}

/**
 * Hack, until we can get mouse tracking working over
 * list box row components, we can at least track them when
 * over our label.  Since the label is full width, look at
 * the mouse position to pick which "side" we're on.
 * Ugh, we only get enter once, would have to track
 * mouseMove to adapt to movement once you're inside.
 */
void MultiSelectDrag::mouseEnter(const juce::MouseEvent& event)
{
    (void)event;
    //Trace(2, "MultiSelectDrag::mouseEnter %s\n", label.toUTF8());
    if (helpArea != nullptr) {
        juce::String key = helpPrefix + "Current";
        helpArea->showHelp(key);
    }
}

void MultiSelectDrag::mouseExit(const juce::MouseEvent& event)
{
    (void)event;
    if (helpArea != nullptr) {
        helpArea->clear();
    }
}

//////////////////////////////////////////////////////////////////////
//
// StringArrayListBox
//
//////////////////////////////////////////////////////////////////////

StringArrayListBox::StringArrayListBox()
{
    box.setModel(this);
    box.setMultipleSelectionEnabled(true);
    addAndMakeVisible(box);
}

StringArrayListBox::~StringArrayListBox()
{
}

// only need one listener for now
void StringArrayListBox::setListener(Listener* l)
{
    listener = l;
}

/**
 * todo: If this is not sorted, then I think the expectation
 * would be that order is significant, which means drag/drop
 * within the listbox could be used to change order.
 *
 * In the past I've used row selection combined with "move up"
 * and "move down" buttons which is ugly.
 */
void StringArrayListBox::setSorted(bool b)
{
    sorted = b;
    if (sorted)
      strings.sort(false);
}

void StringArrayListBox::clear()
{
    strings.clear();
    box.updateContent();
}

void StringArrayListBox::remove(juce::StringArray& values)
{
    // don't have removeArray for some reason
    for (auto s : values)
      strings.removeString(s);

    // unclear if removeStrings will retain sort order
    // of the remaining strings
    
    box.updateContent();
    // looks weird to have lingering selections after this
    box.deselectAllRows();
}

void StringArrayListBox::setValue(juce::StringArray& value)
{
    strings = value;
    if (sorted)
      strings.sort(false);
    box.updateContent();
}

juce::StringArray StringArrayListBox::getValue()
{
    return strings;
}

void StringArrayListBox::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    box.setBounds(area.reduced(1));
}

void StringArrayListBox::paint(juce::Graphics& g)
{
    if (targetActive) {
        g.setColour (juce::Colours::green);
        g.drawRect (getLocalBounds(), 1);
    }
}

// ListBoxModel

int StringArrayListBox::getNumRows()
{
    return strings.size();
}

void StringArrayListBox::paintListBoxItem (int rowNumber, juce::Graphics& g,
                                           int width, int height, bool rowIsSelected)
{
    if (rowIsSelected)
      g.fillAll (juce::Colours::lightblue);

    g.setColour (juce::LookAndFeel::getDefaultLookAndFeel().findColour (juce::Label::textColourId));
    g.setFont ((float) height * 0.7f);

    g.drawText (strings[rowNumber], 5, 0, width, height,
                juce::Justification::centredLeft, true);
}

/**
 * Build the thing the target gets when something is dropped.
 * 
 * from the demo:
 * for our drag description, we'll just make a comma-separated list of the selected row
 * numbers - this will be picked up by the drag target and displayed in its box.
 *
 * In the context of MultiSelectDrag we want to move a set of strings from
 * one list box to another.  The easiest way to do that is to have the description
 * be array of strings.  A CSV is unreliable because an item in the array could contain
 * a comma, and I don't want to mess with delimiters and quoting.
 *
 * Passing just the item numbers like the demo means we have to ask some parent
 * component what those numbers mean.  This might make StringArrayListBox more usable
 * in different contexts, but more work.
 *
 * It is unclear what the side effects of having the description be an arbitrarily
 * long array of arbitrarily long strings would be.
 */
juce::var StringArrayListBox::getDragSourceDescription (const juce::SparseSet<int>& selectedRows)
{
    bool useNumberCsv = false;
    juce::StringArray rows;

    if (useNumberCsv) {
        for (int i = 0; i < selectedRows.size(); ++i)
          rows.add (juce::String (selectedRows[i] + 1));
        return rows.joinIntoString (", ");
    }
    else {
        // could always just pass an array here, but test both
        // in case some future source wants to drag in singles
        if (selectedRows.size() > 1) {
            for (int i = 0; i < selectedRows.size(); ++i)
              rows.add(strings[selectedRows[i]]);
            return rows;
        }
        else {
            return strings[selectedRows[0]];
        }
    }
}

/**
 * Model hook ordinarilly used to change the mouse cursor when it is over
 * a given row.  We're going to use it to change the help text.
 *
 * Could also look at getTooltipForRow, which also looks interesting.
 *
 * Well this doesn't work, this is called for all rows once when you click
 * on a row, not as the mouse hovers over a row.
 */
#if 0
juce::MouseCursor StringArrayListBox::getMouseCursorForRow(int row)
{
    Trace(2, "StringArrayListBox::getMouseCursorForRow %d\n", row);
    return juce::MouseCursor();
}

// see what this does
juce::String StringArrayListBox::getTooltipForRow(int row)
{
    Trace(2, "StringArrayListBox::getTooltipForRow %d\n", row);
    return juce::String("Row " + juce::String(row));
}
#endif

// DragAndDropTarget

bool StringArrayListBox::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details)
{
    (void)details;
    return true;
}

void StringArrayListBox::itemDragEnter (const juce::DragAndDropTarget::SourceDetails& details)
{
    // we are both a source and a target, so don't highlight if we're over oursleves
    // spec is unclear what the sourceComponent will be if this is an item from
    // a ListBox, what you are dragging is some sort of inner component for the ListBox
    // with arbitrary structure between it and the ListBox, comparing against the
    // outer ListBox seems to work
    if (details.sourceComponent != &box) {
        targetActive = true;
        moveActive = false;
        repaint();
    }
    else {
        // moving within ourselves
        moveActive = true;
        targetActive = false;
    }
}

/**
 * If we're dragging within ourselves, give some indication of the insertion point.
 * Actually it doesn't matter if the drag is coming from the outside, still need
 * to be order sensitive unless sorted.  I gave up trying to predict what
 * getInsertionIndexForPosition does.  You can calculate the drop position without that
 * in itemDropped, though it would be nice to draw that usual insertion line between items
 * while the drag is in progress.  Revisit someday...
 */
void StringArrayListBox::itemDragMove (const juce::DragAndDropTarget::SourceDetails& details)
{
    juce::Point<int> pos = details.localPosition;
    // position is "relative to the target component"  in this case the target
    // is the StringArrayListBox which offsets the ListBox by 4 on all sides to draw
    // the drop border, convert wrapper coordinates to ListBox coordinates
    int listBoxX = pos.getX() - box.getX();
    int listBoxY = pos.getY() - box.getY();
    int insertIndex =  box.getInsertionIndexForPosition(listBoxX, listBoxY);
    if (insertIndex != lastInsertIndex) {
        //Trace(2, "Insertion index %ld\n", (long)insertIndex);
        lastInsertIndex = insertIndex;
    }
}

/**
 * If we started a drag, and went off into space without landing on a target, I suppose we
 * could treat this as a special form of move that removes the value from the list.
 * But I don't think we can tell from here, this just means that the mouse left the
 * ListBox, it may come back again.
 */
void StringArrayListBox::itemDragExit (const juce::DragAndDropTarget::SourceDetails& details)
{
    (void)details;
    targetActive = false;
    moveActive = false;
    repaint();
}

/**
 * Something dropped in this ListBox.
 * Since we are both a source and a target, if we drop within oursleves,
 * treat this as a move if the list is ordered.
 *
 * If we are dragging from the outside, convert the source details into
 * a StringArray, add those values to our list, and inform the listener that
 * new values were received.  When used with MultiSelectDrag this will cause
 * those values to be removed from the source ListBox.
 */
void StringArrayListBox::itemDropped (const juce::DragAndDropTarget::SourceDetails& details)
{
    // accept either an array or a single string
    juce::StringArray newValues;
    juce::var stuff = details.description;
    if (stuff.isArray()) {
        // seems like there should be an easier way to do this
        for (int i = 0 ; i < stuff.size() ; i++)
          newValues.add(stuff[i]);
    }
    else {
        newValues.add(stuff);
    }

    if (newValues.size() > 0) {

        if (details.sourceComponent != &box) {
            // dragging in from the outside
            if (sorted) {
                strings.mergeArray(newValues);
                strings.sort(false);
                // merge puts things at the end if they are new and they may not
                // be visible, if we're sorting the sort position of the new row(s)
                // may not be visible either
                // if we drug multiples, focus on the first one
                int firstNewRow = strings.indexOf(newValues[0]);
                box.scrollToEnsureRowIsOnscreen(firstNewRow);
            }
            else {
                // insert them at the drop row
                // multiples are not in any defined order I think,
                // probably the lexical order from the source list, not
                // the order in which they were selected
                int dropRow = getDropRow(details);
                for (auto value : newValues) {
                    strings.insert(dropRow, value);
                    // if you keep inserting at the ssame row, it reverses the order
                    dropRow++;
                }
            }
        
            // MultiSelectDrag will clear selections in the source list
            // which looks more obvious, do the same here since
            // a prior selection isn't really meaningful now?
            box.deselectAllRows();
            box.updateContent();
        
            if (listener != nullptr)
              listener->valuesReceived(this, newValues);
        }
        else if (!sorted) {
            // dragging within ourselves, ignore if we are sorted
            // unclear what it means to move multiples, too many edge cases,
            // just do the first one
            int sourceRow = strings.indexOf(newValues[0]);
            int dropRow = getDropRow(details);
            // StringArray does the work, nice
            strings.move(sourceRow, dropRow);
            box.updateContent();
            // we can't drop without the drop location being visible so
            // don't have to scroll, right?
        }
    }
    targetActive = false;
    moveActive = false;
    repaint();
}

/**
 * Calculate the row where a drop should be inserted when using
 * an unordered list.
 *
 * getInsertionIndexForPosition tracking during itemDragMove was wonky
 * and I never did understand it.  We don't really need that since we have
 * the drop coordinates in details.localPosition and can ask the ListBox
 * for getRowContainingPosition.  Note that localPosition is relative to the
 * DragAndDropTarget which is StringArrayListBox and the ListBox is inset by
 * 4 on all sides to draw a border.  So have to adjust the coordinates to ListBox
 * coordinates when calling getRowCintainingPosition.
 */
int StringArrayListBox::getDropRow(const juce::DragAndDropTarget::SourceDetails& details)
{
    juce::Point<int> pos = details.localPosition;
    int dropX = pos.getX();
    int dropY = pos.getY();
    //Trace(2, "Drop position %ld %ld\n", dropX, dropY);
    dropX -= box.getX();
    dropY -= box.getY();
    //Trace(2, "Drop position within ListBox %ld %ld\n", dropX, dropY);
    int dropRow = box.getRowContainingPosition(dropX, dropY);

    return dropRow;
}

//////////////////////////////////////////////////////////////////////
//
// HelpfulListBox
//
//////////////////////////////////////////////////////////////////////

void HelpfulListBox::mouseEnter(const juce::MouseEvent& event)
{
    //Trace(2, "HelpfulListBox::mouseEnter %s\n", helpName.toUTF8());
    if (helpListener != nullptr)
      helpListener->showHelp(this, helpName);
    
    ListBox::mouseEnter(event);
}

void HelpfulListBox::mouseExit(const juce::MouseEvent& event)
{
    //Trace(2, "HelpfulListBox::mouseExit %s\n", helpName.toUTF8());
    if (helpListener != nullptr)
      helpListener->hideHelp(this, helpName);
    
    ListBox::mouseExit(event);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/


