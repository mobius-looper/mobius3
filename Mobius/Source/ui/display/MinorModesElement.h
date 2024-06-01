/**
 * Status element to display minor modes
 */

#pragma once

#include <JuceHeader.h>

#include "StatusElement.h"

class MinorModesElement : public StatusElement
{
  public:
    
    MinorModesElement(class StatusArea* area);
    ~MinorModesElement();

    void update(class MobiusState* state) override;
    int getPreferredWidth() override;
    int getPreferredHeight() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

	bool mRecording = false;
	bool mOverdub = false;
	bool mMute = false;
	bool mReverse = false;

    int mSpeedToggle = 0;
    int mSpeedOctave = 0;
	int mSpeedStep = 0;
	int mSpeedBend = 0;
    int mPitchOctave = 0;
	int mPitchStep = 0;
	int mPitchBend = 0;
	int mTimeStretch = 0;

	bool mTrackSyncMaster = false;
	bool mOutSyncMaster = false;
    bool mSolo = false;
    bool mGlobalMute = false;
    bool mGlobalPause = false;
    bool mWindow = false;
    
};

    
