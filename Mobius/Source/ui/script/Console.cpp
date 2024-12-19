/**
 * A TextEditor extension that behaves like a command line console
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
#include "Console.h"

Console::Console()
{
    setName("Console");

    setMultiLine(true);
    setReadOnly(false);
    setScrollbarsShown(true);
    setCaretVisible(true);

    // this is what makes it call textEditorReturnKeyPressed
    setReturnKeyStartsNewLine(false);

    // this looks interesting
    // If enabled, right-clicking (or command-clicking on the Mac) will pop up a
    // menu of options such as cut/copy/paste, undo/redo, etc.
    setPopupMenuEnabled (true);

    addListener(this);
}

Console::~Console()
{
}

void Console::add(const juce::String& m)
{
    moveCaretToEnd();
    insertTextAtCaret(m);
    if (!m.endsWithChar('\n')) {
        moveCaretToEnd();
        insertTextAtCaret(juce::newLine);
    }
}

void Console::newline()
{
    moveCaretToEnd();
    insertTextAtCaret ("\n");
    moveCaretToEnd();
}

void Console::prompt()
{
    moveCaretToEnd();
    insertTextAtCaret ("> ");
    moveCaretToEnd();
}

void Console::addAndPrompt(const juce::String& m)
{
    add(m);
    prompt();
}

/**
 * Attempt to figure out what the last line typed into the
 * editor was after the ReturnKeyPressed event.
 * Assumptions:
 *    last character in the contents of the editor back
 *    to the previous newline, then forward over the prompt.
 */
juce::String Console::getLastLine()
{
    juce::String all = getText();
    juce::String line;
    
    int len = all.length();
    int psn = len - 1;
    
    // back up to the last character that is not a newline or space
    while (psn > 0 && (all[psn] == '\n' || all[psn] == ' '))
      psn--;

    if (psn == 0) {
        // weird, should at least have had a prompt
        // leave a message in the buffer?
    }
    else {
        int end = psn;
        // back up to the previous newline
        while (psn > 0 && all[psn] != '\n')
          psn--;
 
        if (all[psn] == '\n')
          psn++;

        // expecting a "> " prompt
        // didn't search backward for that without a preceeding
        // newline in case you wanted to use that in the line
        while (psn < len && (all[psn] == '>' || all[psn] == ' '))
          psn++;

        line = all.substring(psn, end + 1);
    }

    return line;
}

//////////////////////////////////////////////////////////////////////
//
// TextEditor::Listener
//
//////////////////////////////////////////////////////////////////////

void Console::textEditorTextChanged(juce::TextEditor& te)
{
    (void)te;
}

void Console::textEditorReturnKeyPressed(juce::TextEditor& te)
{
    (void)te;
    juce::String line = getLastLine();

    // intercepting Return does not leave a newline in the text
    // add one
    newline();
    
    if (line.length() > 0) {
        if (listener != nullptr)
          listener->consoleLine(line);
    }
    prompt();
}

void Console::textEditorEscapeKeyPressed(juce::TextEditor& te)
{
    (void)te;
    if (listener != nullptr)
      listener->consoleEscape();
}

void Console::textEditorFocusLost(juce::TextEditor& te)
{
    (void)te;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
