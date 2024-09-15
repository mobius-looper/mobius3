
#include "../MobiusView.h"

#include "Colors.h"


/**
 * Select a color to represent the state of a loop.
 *
 * There are several places that used color for loop state, the main
 * LoopMeterElement, the LoopRadar strip element, and the loop bars
 * in the LoopStack.  All had their own inconsistent logic.
 *
 * OG Mobius used these colors fairly consistently.
 *
 *    Black = empty/Reset
 *    Red = recording of any kind
 *    Green = Play
 *    Blue = Mute
 *    Grey = slow speed
 *
 * LoopStack was more complicated since it showed state for loops that
 * were not active.
 *
 * OG LoopStack has these color constants:
 *
 * 	mColor = GlobalPalette->getColor(COLOR_METER, Color::White);
 * 	mSlowColor = GlobalPalette->getColor(COLOR_SLOW_METER, Color::Gray);
 * 	mMuteColor = GlobalPalette->getColor(COLOR_MUTE_METER, Color::Blue);
 * 	mActiveColor = Color::White;
 * 	mPendingColor = Color::Red;
 *
 * The default for COLOR_METER was White but the Palette always used Green
 * from what I can tell.
 * 
 * LoopStack had three things to color:
 *
 *    loop number
 *    bar border
 *    bar content
 *
 * Loop number was white for active and green for inactive.
 * Loop border was white for active and red for pending.
 * Loop bar was green, red, blue, grey, or black if empty.
 *
 */

juce::Colour Colors::getLoopColor(MobiusViewTrack* track)
{
    juce::Colour c = juce::Colours::black;

    // not sure why there were two flags
    if (track->recording || track->overdub) {
        c = juce::Colour(MobiusRed);
    }
    else if (track->mute) {
        c = juce::Colours::blue;
    }
    else if (track->anySpeed) {
        c = juce::Colours::grey;
    }
    else if (track->frames > 0) {
        c = juce::Colour(MobiusGreen);
    }

    return c;
}
