/**
 * Simple extension of TextEditor for my experiments and to capture
 * usage notes.
 *
 * Listener callbacks for textChanged, returnKeyPressed, escapeKeyPressed, focusLost
 *
 * Font and text only apply as new text is added, they are retained in old text.
 *
 * setScrollBarThickness embiggens the scroll bar
 *
 * clear() deletes all text
 * paste() copies the clipboard
 * setCaretPosition
 *
 * setIndents(left, top) - changes the gap at the bottom and left edge
 * setBorder(BorderSize) - changes size of border around the edge
 * setLineSpacing
 *
 * moveCaretToEnd and various cursor positioning
 *
 * addPopupMenuItems - where to the popups come frim?
 * 
 * Not directly related, but I found an interesting tidbit on setOpaque from the example
 * It should be set in components that completely render their area and any components
 * under it would not be visible.  Used to optimize Juce's drawing:
 * "Indicates whether any parts of the component might be transparent.
 * Components that always paint all of their contents with solid colour and
 * thus completely cover any components behind them should use this method
 * to tell the repaint system that they are opaque.
 * this information is used to optimise drawing, because it means that
 * objects underneath opaque windows don't need to be painted.
 * By default, components are considered transparent, unless this is used to
 * make it otherwise.
 * 
 */

#include <JuceHeader.h>

#include "LogPanel.h"

LogPanel::LogPanel()
{
    // see notes on opaque above
    // oh, this does the opposite of what I thought, if you set this you MUST
    // paint out the entire area
    //setOpaque(true);
    
    // always want multiple lines
    setMultiLine (true);
    // used in the example, but I don't think it's relevant if read-only?
    setReturnKeyStartsNewLine (true);
    // only for logging
    setReadOnly (true);
    // oh yeah, bring on the scroll
    setScrollbarsShown (true);
    // read-only so no caret
    // a side effect of this or maybe ReadOnly is that you get no mouse cursor
    // while over this component which is annoying
    setCaretVisible (true);

    // this looks interesting
    // If enabled, right-clicking (or command-clicking on the Mac) will pop up a
    // menu of options such as cut/copy/paste, undo/redo, etc.
    setPopupMenuEnabled (true);

    // colors from the example, start with these
    // other colors are textColourId, highlightColourId, highlightedTextColourId
    // focusedOutlineColourId
    // can also change caret colours using CaretComponent::caretColourId

    // can be transparent whatever that means
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x32ffffff));
    // if not transparent, draws a box around the edge
    // also focusedOutlineColourId is a different color when focused
    setColour (juce::TextEditor::outlineColourId, juce::Colour (0x1c000000));
    // if non-transparent, draws an inner shadow around the edge 
    setColour (juce::TextEditor::shadowColourId, juce::Colour (0x16000000));

    // hmm, when I added this to a beige panel, it just showed in a lighter shade
    // of beige, in the demo the main components has a black backround and the log
    // was a little lighter so the demo back ground colour must have transparency
    // in it.  this is interesting, but I don't have time to explore it yet, just
    // fix a color
    setColour(juce::TextEditor::backgroundColourId, juce::Colours::grey);
    

    // textColourId "used when text is added", it does not change the colour of
    // existing text so this could be nice for formatting log words
    // can use applyColourToAllText to change all existing text

    // highlightColourId "fill the background of highlighted sections"
    // can be transparent if you don't want highlighting
}

LogPanel::~LogPanel()
{
}

void LogPanel::add(const juce::String& m)
{
    moveCaretToEnd();
    insertTextAtCaret (m + juce::newLine);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
