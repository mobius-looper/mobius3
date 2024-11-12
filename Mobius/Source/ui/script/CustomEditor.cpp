
#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "CustomEditor.h"

CustomEditor::CustomEditor()
{
    setMultiLine(true);
    setReturnKeyStartsNewLine(true);
    setTabKeyUsedAsCharacter(true);
    setReadOnly(false);
    setScrollbarsShown(true);
    setCaretVisible(true);

    // let the parent pay attention to this
    //addListener(this);
    addKeyListener(this);

    selecting = false;

    // wire this on for now
    emacs = true;
}

CustomEditor::~CustomEditor()
{
}

void CustomEditor::setEmacsMode(bool b)
{
    emacs = b;
}

/**
 * TextEditor::Listener
 */
void CustomEditor::textEditorTextChanged(juce::TextEditor& te)
{
    (void)te;
    Trace(2, "CustomEditor: Text changed");
}

bool CustomEditor::keyPressed(const juce::KeyPress& key, juce::Component* c)
{
    (void)c;
    bool handled = false;

    if (emacs) {
        //juce::String desc = key.getTextDescription();
        //Trace(2, "CustomEditor::keyPressed %s", desc.toUTF8());

        int code = key.getKeyCode();
        juce::ModifierKeys mods = key.getModifiers();
        int raw = mods.getRawFlags();
        
        if (raw == juce::ModifierKeys::ctrlModifier) {
            handled = true;
            //Trace(2, "Ctrl %d", code);
            
            if (code == 'A') {
                moveCaretToStartOfLine(selecting);
            }
            else if (code == 'E') {
                moveCaretToEndOfLine(selecting);
            }
            else if (code == 'F') {
                moveCaretRight(false, selecting);
            }
            else if (code == 'B') {
                moveCaretLeft(false, selecting);
            }
            else if (code == 'P') {
                moveCaretUp(selecting);
            }
            else if (code == 'N') {
                moveCaretDown(selecting);
            }
            else if (code == 'V') {
                pageDown(selecting);
            }
            else if (code == 'X') {
                // you do ctrl-x/s reflexively and since this
                // will compile probably best not to do it here
            }
            else if (code == 'S') {
                // with ctrl-x does save
                // by itself search
            }
            else if (code == ' ') {
                selecting = true;
            }
            else if (code == 'G') {
                selecting = false;
                setHighlightedRegion(juce::Range<int>());
            }
            else if (code == 'W') {
                cut();
            }
            else if (code == 'D') {
                // delete character
                deleteForwards(false);
            }
            else if (code == 'K') {
                // delete line
                deleteForwards(false);
            }
            else if (code == 'Y') {
                // yank aka copy from clipboard if you
                // don't have a copy list
                paste();
            }
        }
        else if (raw == juce::ModifierKeys::altModifier) {
            handled = true;
            //Trace(2, "Alt %d", code);
            if (code == 'B') {
                moveCaretLeft(true, selecting);
            }
            else if (code == 'F') {
                moveCaretRight(true, selecting);
            }
            else if (code == 'V') {
                pageUp(selecting);
            }
            else if (code == 'N') {
                // there are no "selecting" variants of these
                scrollDown();
            }
            else if (code == 'P') {
                scrollUp();
            }
            else if (code == 'R') {
                // revert source
            }
            else if (code == 'W') {
                // copy and clear selection
                copy();
            }
            else if (code == 'D') {
                // delete word forward
                deleteForwards(true);
            }
        }
        else if ((raw & juce::ModifierKeys::altModifier) &&
                 (raw & juce::ModifierKeys::shiftModifier)) {
            handled = true;
            // > comes in as unshifted .
            if (code == '.') {
                moveCaretToEnd(selecting);
            }
            // < comes in as unshifted ,
            else if (code == ',') {
                moveCaretToTop(selecting);
            }
            
        }
        
    }
    
    // return true to eat keys
    return handled;
}

