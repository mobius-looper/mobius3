
#include "TrackState.h"

const char* TrackState::getModeName(TrackState::Mode amode)
{
    const char* name = "???";
    switch (amode) {
        case TrackState::ModeUnknown: name = "Unknown"; break;
        case TrackState::ModeReset: name = "Reset"; break;
        case TrackState::ModeSynchronize: name = "Synchronize"; break;
        case TrackState::ModeRecord: name = "Record"; break;
        case TrackState::ModePlay: name = "Play"; break;
        case TrackState::ModeOverdub: name = "Overdub"; break;
        case TrackState::ModeMultiply: name = "Multiply"; break;
        case TrackState::ModeInsert: name = "Insert"; break;
        case TrackState::ModeReplace: name = "Replace"; break;
        case TrackState::ModeMute: name = "Mute"; break;

        case TrackState::ModeConfirm: name = "Confirm"; break;
        case TrackState::ModePause: name = "Pause"; break;
        case TrackState::ModeStutter: name = "Stutter"; break;
        case TrackState::ModeSubstitute: name = "Substitute"; break;
        case TrackState::ModeThreshold: name = "Threshold"; break;

        // old Mobius modes, may not need
        case TrackState::ModeRehearse: name = "Rehearse"; break;
        case TrackState::ModeRehearseRecord: name = "RehearseRecord"; break;
        case TrackState::ModeRun: name = "Run"; break;
        case TrackState::ModeSwitch: name = "Switch"; break;

        case TrackState::ModeGlobalReset: name = "GlobalReset"; break;
        case TrackState::ModeGlobalPause: name = "GlobalPause"; break;
        case TrackState::ModeGlobalMute: name = "GlobalMute"; break;
    }
    return name;
}
