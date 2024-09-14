/**
 * Simple audio level meter for track input.
 *
 * Ugh, I wanted this to contain an AudioMeter like
 * StripOutputMeter does, but that makes it intercept
 * mouse events which fucks up the usual mouse sensitivity
 * that status elements have.  Similar to the floating strip
 * with rotaries in them.
 *
 * REALLY need to find a way to forward mouse events from child
 * components.  Until then, have to duplicate much of what
 * AudioMeter does.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/ModeDefinition.h"
#include "../MobiusView.h"

#include "Colors.h"
#include "StatusArea.h"
#include "AudioMeterElement.h"

const int AudioMeterPreferredWidth = 200;
const int AudioMeterPreferredHeight = 20;

AudioMeterElement::AudioMeterElement(StatusArea* area) :
    StatusElement(area, "AudioMeterElement")
{
    // didn't seem to change in old chde
    range = (1024 * 8) - 1;
}

AudioMeterElement::~AudioMeterElement()
{
}

int AudioMeterElement::getPreferredHeight()
{
    return AudioMeterPreferredHeight;
}

int AudioMeterElement::getPreferredWidth()
{
    return AudioMeterPreferredWidth;
}

void AudioMeterElement::resized()
{
}

const int AudioMeterElementInset = 2;

void AudioMeterElement::update(MobiusView* view)
{
    int value = view->track->inputMonitorLevel;
    
	if (savedValue != value && value >= 0 && value <= range) {
		savedValue = value;

        // the old way, I don't like how we're making assumptions
        // about repait based on the current width what if width
        // isn't set yet, or changes between now and paint?
        
		// typically get a lot of low level noise which flutters
		// the value but is not actually visible, remember the last
		// level and don't repaint unless it changes
		int width = getWidth() - (AudioMeterElementInset * 2);
        int level = (int)(((float)width / (float)range) * value);
		if (level != savedLevel) {
			savedLevel = level;
            repaint();
		}
	}
}

/**
 * If we override paint, does that mean we control painting
 * the children, or is that going to cascade?
 */
void AudioMeterElement::paint(juce::Graphics& g)
{
    // borders, labels, etc.
    StatusElement::paint(g);
    if (!isIdentify()) {

        // sigh, have to duplicate what AudioMeter does until
        // we can share code without messing up the component
        // hierarchy and mouse sensitivity

        // getting a one line turd on the left, even when level is zero
        // AudioMeterElement also has that problem, but less often
        // also, are we sure this will clear the background before repaining
        // the new level?
        bool forceClear = true;

        if (forceClear) {
            g.setColour(juce::Colours::black);
            g.fillRect(AudioMeterElementInset,
                       AudioMeterElementInset,
                       getWidth() - (AudioMeterElementInset * 2),
                       getHeight() - (AudioMeterElementInset * 2));
        }
      
        if (savedLevel > 0) {
            g.setColour(juce::Colour(MobiusGreen));
            g.fillRect(AudioMeterElementInset,
                       AudioMeterElementInset,
                       savedLevel,
                       getHeight() - (AudioMeterElementInset * 2));
        }
        else if (!forceClear) {
            g.setColour(juce::Colours::black);
            g.fillRect(AudioMeterElementInset,
                       AudioMeterElementInset,
                       getWidth() - (AudioMeterElementInset * 2),
                       getHeight() - (AudioMeterElementInset * 2));
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
