/**
 * Status element to display the tempo from the plugin host or
 * from MIDI clocks.
 */

#pragma once

#include <JuceHeader.h>

#include "../../model/SyncConstants.h"
#include "StatusElement.h"

class TempoElement : public StatusElement
{
  public:
    
    TempoElement(class StatusArea* area);
    ~TempoElement();

    void update(class MobiusView* view) override;
    int getPreferredWidth() override;
    int getPreferredHeight() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    SyncSource mSyncSource = SyncSourceNone;
    SyncUnit mSyncUnit = SyncUnitBeat;
    TrackSyncUnit mTrackSyncUnit = TrackUnitLoop;
	int mTempo = 0;
	int mBeat = 0;
	int mBar = 0;

};

    
