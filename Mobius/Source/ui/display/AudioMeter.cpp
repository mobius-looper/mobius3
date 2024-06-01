/**
 * Simple audio level meter for track input or output levels.
 *
 * Adapted from AudioMeter in old code.
 * Old code comments:
 *
 *  // this seems to be too sensitive, need a trim control?
 * 	//mMeter->setRange((1024 * 32) - 1);
 * 	setRange((1024 * 8) - 1);
 *
 * mRange seems to set the granularity or sensitivity.
 *
 * This decides what level to display:
 *
 * 	if (mValue != i && i >= 0 && i <= mRange) {
 * 		mValue = i;
 * 
 * 		// typically get a lot of low level noise which flutters
 * 		// the value but is not actually visible, remember the last
 * 		// level and don't repaint unless it changes
 * 		int width = mBounds.width - 4;
 *         int level = (int)(((float)width / (float)mRange) * mValue);
 * 		if (level != mLevel) {
 * 			mLevel = level;
 * 			if (isEnabled())
 * 			  invalidate();
 * 		}
 * 	}
 *
 * And this paints it:
 *
 * 			mLevel = (int)(((float)b.width / (float)mRange) * mValue);
 * 
 * 			if (mLevel > 0) {
 * 				g->setColor(mMeterColor);
 * 				g->fillRect(b.x, b.y, mLevel, b.height);
 * 			}
 *
 * So mLevel is the width of the level rectangle to draw taken as a
 * raction of the actual width times the value
 *
 * The value in MobiusState is calculated down in Stream as:
 *  int OutputStream::getMonitorLevel()
 *  {
 *   	// convert to 16 bit integer
 *   	return (int)(mMaxSample * 32767.0f);
 *  }
 *
 * And mMaxSample comes directly from the audio stream
 * Not following the old math here, but we can make the same assumption.
 *
 * And finally the value is set here:
 *  
 *     mMeter->update(tstate->inputMonitorLevel);
 *
 * So don't trigger a repaint just when the raw sample changes because
 * that would be way too bouncy.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "Colors.h"
#include "AudioMeter.h"

AudioMeter::AudioMeter()
{
    // didn't seem to change in old chde
    range = (1024 * 8) - 1;
}

AudioMeter::~AudioMeter()
{
}

void AudioMeter::resized()
{
}
const int AudioMeterInset = 2;

void AudioMeter::update(int value)
{
	if (savedValue != value && value >= 0 && value <= range) {
		savedValue = value;

        // the old way, I don't like how we're making assumptions
        // about repaint based on the current width what if width
        // isn't set yet, or changes between now and paint?
        
		// typically get a lot of low level noise which flutters
		// the value but is not actually visible, remember the last
		// level and don't repaint unless it changes
		int width = getWidth() - (AudioMeterInset * 2);
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
void AudioMeter::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(MobiusRed));

    // getting a one line turd on the left, even when level is zero
    // AudioMeterElement also has that problem, but less often
    // also, are we sure this will clear the background before repaining
    // the new level?
    bool forceClear = true;

    if (forceClear) {
        g.setColour(juce::Colours::black);
        g.fillRect(AudioMeterInset,
                   AudioMeterInset,
                   getWidth() - (AudioMeterInset * 2),
                   getHeight() - (AudioMeterInset * 2));
    }
      
    if (savedLevel > 0) {
        g.fillRect(AudioMeterInset,
                   AudioMeterInset,
                   savedLevel,
                   getHeight() - (AudioMeterInset * 2));
    }
    else if (!forceClear) {
        g.setColour(juce::Colours::black);
        g.fillRect(AudioMeterInset,
                   AudioMeterInset,
                   getWidth() - (AudioMeterInset * 2),
                   getHeight() - (AudioMeterInset * 2));
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
