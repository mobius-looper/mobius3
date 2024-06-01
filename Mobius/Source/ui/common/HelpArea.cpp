
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/HelpCatalog.h"

#include "HelpArea.h"


HelpArea::HelpArea()
{
    // configure the TextEditor with the usual read-only options
    // consider factoring this out into a component

    area.setReadOnly (true);
    area.setMultiLine (true);
    // may not want this, help text shouldn't be long
    area.setScrollbarsShown (true);
    // read-only so no caret
    area.setCaretVisible (false);

    // this looks interesting
    // If enabled, right-clicking (or command-clicking on the Mac) will pop up a
    // menu of options such as cut/copy/paste, undo/redo, etc.
    area.setPopupMenuEnabled (true);

    // colors from the example, start with these
    // other colors are textColourId, highlightColourId, highlightedTextColourId
    // focusedOutlineColourId
    // can also change caret colours using CaretComponent::caretColourId

    // can be transparent whatever that means
    area.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x32ffffff));
    // if not transparent, draws a box around the edge
    // also focusedOutlineColourId is a different color when focused
    area.setColour (juce::TextEditor::outlineColourId, juce::Colour (0x1c000000));
    // if non-transparent, draws an inner shadow around the edge 
    area.setColour (juce::TextEditor::shadowColourId, juce::Colour (0x16000000));

    // hmm, when I added this to a beige panel, it just showed in a lighter shade
    // of beige, in the demo the main components has a black backround and the log
    // was a little lighter so the demo back ground colour must have transparency
    // in it.  this is interesting, but I don't have time to explore it yet, just
    // fix a color
    setBackground(juce::Colours::grey);

    // todo: think about fonts
    // default seems enough but help text will typically be only a few lines of
    // text, should make it larger if the availble height is large?

    addAndMakeVisible(area);
}

/**
 * Set the help catalog to use.  We do not own this.
 */
void HelpArea::setCatalog(HelpCatalog* cat)
{
    catalog = cat;
}

void HelpArea::setBackground(const juce::Colour color)
{
    // todo: if this is set light, invert the text so it remains visible
    area.setColour(juce::TextEditor::backgroundColourId, color);
}
    
/**
 * Show raw text without looking for it in the catalog.
 */
void HelpArea::showText(juce::String text)
{
    // don't need this, setText will replace existing text
    // area.clear();
    
    // second arg is sendTextChangeMessage which sends change message
    // to all the listeners, which we don't have
    area.setText(text, false);
}

/**
 * Show some help text.  This gets called a lot as the mouse
 * hovers over things.  Remember the key of the last thing we've shown
 * and don't duplicate it.
 */
void HelpArea::showHelp(juce::String key)
{
    if (catalog == nullptr) {
        showText("No help catalog: " + key);
    }
    else if (key != lastKey) {
        juce::String help = catalog->get(key);
        if (help.length() > 0)
          showText(help);
        else {
            // normally should just be quiet but I'm looking for
            // missing things in the catalog
            showText("No help: " + key);
        }
        lastKey = key;
    }
}

void HelpArea::resized()
{
    area.setBounds(getLocalBounds());
}

void HelpArea::paint(juce::Graphics &g)
{
    (void)g;
}


