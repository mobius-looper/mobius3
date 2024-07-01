/**
 * Simple panel to display a random alert message.
 * 
 * This one is a little unusual in that we can in theory want to show several
 * alerts at the same time, or a new alert after an old one is already
 * showing.
 *
 * The original Alerter did this by dynamically allocating them and keeping
 * a garbage collection list, which was overkill.
 *
 * Here if the alert window is already displayed, we just replace the text.
 * If you want to get fancy, successive alerts could stack within the panel
 * but could require scrolling if there are a lot of them.
 *
 * Could also queue them, and when "Ok" is clicked, it displays the next
 * message in the queue rather than closing.  In practice, multiple alerts
 * would only happen in scripts or if something is horribly wrong, so keeping
 * them all in a scrolling window let's you see all of them, and dismiss them
 * all at once rather than having to click Ok a bunch of times.
 * 
 */

#include <JuceHeader.h>

#include "../Supervisor.h"

#include "JuceUtil.h"
#include "BasePanel.h"
#include "AlertPanel.h"

void AlertPanel::show(juce::String message)
{
    content.setMessage(message);
    content.resized();
    JuceUtil::centerInParent(this);
    BasePanel::show();
}

/**
 * What we have here is an area immediately under the title bar and above the Ok button.
 * What I'd like is to have the text centered, but allow it to be broken up over several lines.
 *
 * Using a Label that fills the available space with Justification::centred
 * does a pretty good job of that.  It breaks up the lines, but then centers each
 * line which doesn't look bad.  But if we start allowing multiple messages,
 * it would be better to have each in a filled paragraph with the entire paragraph
 * centered, rather than each line.
 */
AlertContent::AlertContent()
{
    text.setColour(juce::Label::ColourIds::textColourId, juce::Colours::red);
    text.setFont(juce::Font(FontHeight));
    text.setJustificationType(juce::Justification::centred);
    
    addAndMakeVisible(text);
}

void AlertContent::resized()
{
    text.setBounds(getLocalBounds());
}
