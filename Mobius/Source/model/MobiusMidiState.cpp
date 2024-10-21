
#include "MobiusMidiState.h"

const char* MobiusMidiState::getModeName(MobiusMidiState::Mode amode)
{
    const char* name = "???";
    switch (amode) {
        case MobiusMidiState::ModeReset: name = "Reset"; break;
        case MobiusMidiState::ModeSynchronize: name = "Synchronize"; break;
        case MobiusMidiState::ModeRecord: name = "Record"; break;
        case MobiusMidiState::ModePlay: name = "Play"; break;
        case MobiusMidiState::ModeOverdub: name = "Overdub"; break;
        case MobiusMidiState::ModeMultiply: name = "Multiply"; break;
        case MobiusMidiState::ModeInsert: name = "Insert"; break;
        case MobiusMidiState::ModeReplace: name = "Replace"; break;
        case MobiusMidiState::ModeMute: name = "Mute"; break;

        case MobiusMidiState::ModeConfirm: name = "Confirm"; break;
        case MobiusMidiState::ModePause: name = "Pause"; break;
        case MobiusMidiState::ModeStutter: name = "Stutter"; break;
        case MobiusMidiState::ModeSubstitute: name = "Substitute"; break;
        case MobiusMidiState::ModeThreshold: name = "Threshold"; break;

        // old Mobius modes, may not need
        case MobiusMidiState::ModeRehearse: name = "Rehearse"; break;
        case MobiusMidiState::ModeRehearseRecord: name = "RehearseRecord"; break;
        case MobiusMidiState::ModeRun: name = "Run"; break;
        case MobiusMidiState::ModeSwitch: name = "Switch"; break;

        case MobiusMidiState::ModeGlobalReset: name = "GlobalReset"; break;
        case MobiusMidiState::ModeGlobalPause: name = "GlobalPause"; break;
    }
    return name;
}
