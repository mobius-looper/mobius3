/**
 * Display element to display minor modes.
 *
 * There are a bunch of these and they display in a single row, typically only
 * a few are visible at a time.  I don't like how this works, but it follows the
 * old design and works well enough for now.  I'd rather these have fixed locations
 * kind of like a car dashboard, but that takes up more space.  Icons could be cool
 * but then everyone forgets what they mean.
 */

#include <JuceHeader.h>

#include "../../model/MobiusState.h"

#include "Colors.h"
#include "StatusArea.h"
#include "MinorModesElement.h"

MinorModesElement::MinorModesElement(StatusArea* area) :
    StatusElement(area, "MinorModesElement")
{
    mouseEnterIdentify = true;
}

MinorModesElement::~MinorModesElement()
{
}

int MinorModesElement::getPreferredHeight()
{
    return 20;
}

/**
 * Old code did a lot of analysis on the text sizes of the most important
 * mode combinations.  Just make it wide enough to have a few since most aren't used.
 */
int MinorModesElement::getPreferredWidth()
{
    return 400;
}

/**
 * Annoyingly large number of things to track here.
 */
void MinorModesElement::update(MobiusState* state)
{
    MobiusTrackState* track = &(state->tracks[state->activeTrack]);
    MobiusLoopState* loop = &(track->loops[track->activeLoop]);

    bool windowing = (loop->windowOffset >= 0);

	if (state->globalRecording != mRecording ||
        track->reverse != mReverse ||
		loop->overdub != mOverdub ||
		loop->mute != mMute ||
        track->speedToggle != mSpeedToggle ||
		track->speedOctave != mSpeedOctave ||
		track->speedStep != mSpeedStep ||
		track->speedBend != mSpeedBend ||
		track->pitchOctave != mPitchOctave ||
		track->pitchStep != mPitchStep ||
		track->pitchBend != mPitchBend ||
		track->timeStretch != mTimeStretch ||

		track->outSyncMaster != mOutSyncMaster ||	
		track->trackSyncMaster != mTrackSyncMaster ||
        track->solo != mSolo ||
        track->globalMute != mGlobalMute ||
        track->globalPause != mGlobalPause ||
        windowing != mWindow) {

		mRecording = state->globalRecording;
		mReverse = track->reverse;
		mOverdub = loop->overdub;
		mMute = loop->mute;
        mSpeedToggle = track->speedToggle;
        mSpeedOctave = track->speedOctave;
		mSpeedStep = track->speedStep;
		mSpeedBend = track->speedBend;
        mPitchOctave = track->pitchOctave;
		mPitchStep = track->pitchStep;
		mPitchBend = track->pitchBend;
		mTimeStretch = track->timeStretch;

		mOutSyncMaster = track->outSyncMaster;
		mTrackSyncMaster = track->trackSyncMaster;
        mSolo = track->solo;
        mGlobalMute = track->globalMute;
        mGlobalPause = track->globalPause;
        mWindow = windowing;

        repaint();
	}
}

void MinorModesElement::resized()
{
}

void MinorModesElement::paint(juce::Graphics& g)
{
    // borders, labels, etc.
    StatusElement::paint(g);
    if (isIdentify()) return;
    
    g.setColour(juce::Colour((juce::uint32)MobiusBlue));
    juce::Font font = juce::Font(getHeight() * 0.8f);
    g.setFont(font);

    juce::String modes;

    if (mOverdub) modes += "Overdub ";
    if (mMute) modes += "Mute ";
    if (mReverse) modes += "Reverse ";

    if (mSpeedOctave != 0)
      modes += "SpeedOct " + juce::String(mSpeedOctave) + " ";

    if (mSpeedStep != 0) {
        // factor out the toggle since they
        // are percived at different things
        int step = mSpeedStep - mSpeedToggle;
        if (step != 0)
          modes += "SpeedStep " + juce::String(step) + " ";
    }

    if (mSpeedToggle != 0)
      modes += "SpeedToggle " + juce::String(mSpeedToggle) + " ";

    // This can also be a knob so we don't need
    // this but I'm not sure people want to waste
    // space for a knob that's too fine grained
    // to use from the UI anyway.
    if (mSpeedBend != 0)
      modes += "SpeedBend " + juce::String(mSpeedBend) + " ";

    if (mPitchOctave != 0)
      modes += "PitchOctave " + juce::String(mPitchOctave) + " ";
    
    if (mPitchStep != 0)
      modes += "PitchStep " + juce::String(mPitchStep) + " ";
    
    if (mPitchBend != 0)
      modes += "PitchBend " + juce::String(mPitchBend) + " ";

    if (mTimeStretch != 0)
      modes += "TimeStretch " + juce::String(mTimeStretch) + " ";

    // forget why I had the combo here, and why they're a mutex
    if (mTrackSyncMaster && mOutSyncMaster) {
        modes+= "Sync Master ";
    }
    else if (mTrackSyncMaster) {
        modes += "Track Sync Master ";
    }
    else if (mOutSyncMaster) {
        modes += "MIDI Sync Master ";
    }

    // the state flag has been "recording" but the mode was displayed
    // as "Capture" so this must have only been used for Capture/Bounce
    if (mRecording)
      modes += "Capture ";

    // this would be better as something in the track strip like DAWs do
    if (mSolo)
      modes += "Solo ";

    // this is a weird one, it will be set during Solo too...
    if (mGlobalMute && !mSolo) {
        modes += "Global Mute ";
    }

    if (mGlobalPause) {
        modes += "Global Pause ";
    }

    // this is "loop window" mode
    if (mWindow) {
        modes += "Windowing ";
    }

    g.drawText(modes, 0, 0, getWidth(), getHeight(), juce::Justification::left);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
