/**
 * A subcomponent of LooperScheduler that deals with the complexity of changing
 * from one loop to another within a track.  Like LooperScheduler, it uses LooperTrack
 * and should be kept generic so that it may be the common switch scheduler for
 * both audio and midi loops.
 *
 * Changing audio loops involves some legacy EDP options like EmptyLoopAction
 * SwitchDuration, and SwitchLocation.
 *
 * Changing MIDI loops involves "follow" options where tracks behave more like
 * banks of pre-recorded MIDI sequences than EDP-style audio loops.
 *
 * Loop switch can be quantized in various ways and during the quantization
 * period, aka "Loop Switch Mode", actions that come in are stacked and execute
 * after the loop switch.   
 *
 */

#pragma once

#include "../../model/ParameterConstants.h"

class LooperSwitcher
{
  public:

    LooperSwitcher(class LooperScheduler& s);
    ~LooperSwitcher();
    void initialize();

    bool isSwitching();
    void scheduleSwitch(class UIAction* a);
    void handleSwitchModeAction(class UIAction* a);
    void doSwitchNow(class UIAction* a);
    void doSwitchEvent(class TrackEvent* e, int target);

    void leaderEvent(class TrackProperties& props);
    
  private:

    LooperScheduler& scheduler;

    int findLeader();
    int getSwitchTarget(class UIAction* a);
    int getQuantizedFrame(SwitchQuantize squant);
    QuantizeMode convert(SwitchQuantize squant);
    bool setupEmptyLoop(int previousLoop);
};
