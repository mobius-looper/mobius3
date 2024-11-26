
#include "MobiusState.h"

const char* MobiusState::getModeName(MobiusState::Mode amode)
{
    const char* name = "???";
    switch (amode) {
        case MobiusState::ModeUnknown: name = "Unknown"; break;
        case MobiusState::ModeReset: name = "Reset"; break;
        case MobiusState::ModeSynchronize: name = "Synchronize"; break;
        case MobiusState::ModeRecord: name = "Record"; break;
        case MobiusState::ModePlay: name = "Play"; break;
        case MobiusState::ModeOverdub: name = "Overdub"; break;
        case MobiusState::ModeMultiply: name = "Multiply"; break;
        case MobiusState::ModeInsert: name = "Insert"; break;
        case MobiusState::ModeReplace: name = "Replace"; break;
        case MobiusState::ModeMute: name = "Mute"; break;

        case MobiusState::ModeConfirm: name = "Confirm"; break;
        case MobiusState::ModePause: name = "Pause"; break;
        case MobiusState::ModeStutter: name = "Stutter"; break;
        case MobiusState::ModeSubstitute: name = "Substitute"; break;
        case MobiusState::ModeThreshold: name = "Threshold"; break;

        // old Mobius modes, may not need
        case MobiusState::ModeRehearse: name = "Rehearse"; break;
        case MobiusState::ModeRehearseRecord: name = "RehearseRecord"; break;
        case MobiusState::ModeRun: name = "Run"; break;
        case MobiusState::ModeSwitch: name = "Switch"; break;

        case MobiusState::ModeGlobalReset: name = "GlobalReset"; break;
        case MobiusState::ModeGlobalPause: name = "GlobalPause"; break;
    }
    return name;
}
