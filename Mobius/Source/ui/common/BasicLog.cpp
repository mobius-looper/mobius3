/**
 * A TextEditor extension with the usual settings to make it a read-only
 * log for messages.
 *
 * Options of interest:
 *
 * setIndents(left, top) - changes the gap at the bottom and left edge
 * setBorder(BorderSize) - changes size of border around the edge
 * setLineSpacing
 *
 * Can be transparent whatever that means
 *   setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x32ffffff));
 *
 * If not transparent, draws a box around the edge
 * Also focusedOutlineColourId is a different color when focused
 *   setColour (juce::TextEditor::outlineColourId, juce::Colour (0x1c000000));
 *
 * If not transparent, draws an inner shadow around the edge 
 *   setColour (juce::TextEditor::shadowColourId, juce::Colour (0x16000000));
 * 
 */

#include <JuceHeader.h>
#include "BasicLog.h"

BasicLog::BasicLog()
{
    setName("BasicLog");

    setMultiLine(true);
    setReadOnly(true);
    setScrollbarsShown(true);
    setCaretVisible(false);
}

BasicLog::~BasicLog()
{
}

/**
 * Trace messages usually come in with a trailing newline.
 * Random code message usually don't.
 */
void BasicLog::add(const juce::String& m)
{
    moveCaretToEnd();
    insertTextAtCaret(m);
    if (!m.endsWithChar('\n')) {
        moveCaretToEnd();
        insertTextAtCaret(juce::newLine);
    }
}

void BasicLog::append(const juce::String& m)
{
    moveCaretToEnd();
    insertTextAtCaret(m);
}
