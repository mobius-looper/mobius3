
#include <JuceHeader.h>

// dependency on the Random utility
// rather not have this, but it's somewhat important that you
// don't seed this often, maybe move this to MslContext and let it deal with
// the containing application environment
#include "../util/Util.h"

#include "MslValue.h"
#include "MslEnvironment.h"
#include "MslSession.h"

#include "MslStandardLibrary.h"

bool MslStandardLibrary::RandSeeded = false;

MslLibraryDefinition MslLibraryDefinitions[] = {

    {"EndSustain", MslFuncEndSustain},
    {"EndRepeat", MslFuncEndRepeat},
    {"Time", MslFuncTime},
    {"Rand", MslFuncRand},
    {"SampleRate", MslFuncSampleRate},
    {"Tempo", MslFuncTempo},
    
    {nullptr, MslFuncNone}
};

MslLibraryDefinition* MslStandardLibrary::find(juce::String name)
{
    MslLibraryDefinition* def = nullptr;

    for (int i = 0 ; MslLibraryDefinitions[i].name != nullptr ; i++) {
        if (strcmp(MslLibraryDefinitions[i].name, name.toUTF8()) == 0) {
            def = &(MslLibraryDefinitions[i]);
            break;
        }
    }
    return def;
}

MslValue* MslStandardLibrary::call(MslSession* s, MslLibraryId id, MslValue* arguments)
{
    MslValue* result = nullptr;

    switch (id) {
        case MslFuncNone: break;
        case MslFuncEndSustain: result = EndSustain(s, arguments) ; break;
        case MslFuncEndRepeat: result = EndRepeat(s, arguments) ; break;
        case MslFuncTime: result = Time(s, arguments) ; break;
        case MslFuncRand: result = Rand(s, arguments) ; break;
        case MslFuncSampleRate: result = SampleRate(s, arguments); break;
        case MslFuncTempo: result = Tempo(s, arguments) ; break;
    }

    return result;
}

MslValue* MslStandardLibrary::Time(MslSession* s, MslValue* arguments)
{
    (void)arguments;
    MslValue* v = s->getEnvironment()->allocValue();
    v->setInt(juce::Time::getMillisecondCounter());
    return v;
}

/**
 * This one takes two arguments (low,high)
 * Since internals don't have signatures yet, we won't have caught missing
 * arguments by now, but it seems reasonable for this one to have default behavior
 *
 *    Rand - random between 0 and 127
 *    Rand(x) - random between 0 and x
 *    Rand(x,y) - random between x and y
 *
 * If low >= high the result is low
 * Core implementation is in util/Util so it can be used outside
 * MSL without contention on seeding.
 */
MslValue* MslStandardLibrary::Rand(MslSession* s, MslValue* arguments)
{
    int low = 0;
    int high = 127;

    if (arguments != nullptr) {
        MslValue* second = arguments->next;
        if (second == nullptr)
          high = arguments->getInt();
        else {
            low = arguments->getInt();
            high = second->getInt();
        }
    }

    int value = Random(low, high);

    MslValue* v = s->getEnvironment()->allocValue();
    v->setInt(value);
    return v;
}

MslValue* MslStandardLibrary::SampleRate(MslSession* s, MslValue* arguments)
{
    (void)arguments;
    MslValue* v = s->getEnvironment()->allocValue();
    v->setInt(s->getContext()->mslGetSampleRate());
    return v;
}

/**
 * This is only necessary because MSL math is all integer based at the moment.
 * Hold off on generalized float support for awhile, though it wouldn't be that
 * hard for / to yield floats then then add Floor and Ceil
 *
 * Tempo takes as arguments the results of two calls to Time
 * In order to calculate a tempo it needs to know the sample rate, which has to be
 * provided by the MslContext.
 *
 * At 60 BPM, each beat is one second long, or 1000 milliseconds.
 *
 * Tempo is calculated as:
 *
 *     ((endMsec - startMsec) / 1000 * 60)
 */
MslValue* MslStandardLibrary::Tempo(MslSession* s, MslValue* arguments)
{
    float tempo = 0.0f;
    
    if (arguments == nullptr) {
        s->addError("Tempo: Missing time arguments");
    }
    else {
        MslValue* second = arguments->next;
        if (second == nullptr) {
            s->addError("Tempo: Missing second time argument");
        }
        else {
            int start = arguments->getInt();
            int end = second->getInt();
            if (end > start) {
                tempo = 60000.0f / (float)(end - start);

                // todo: need some min/max wrapping
            }
        }
    }
        
    MslValue* result = s->getEnvironment()->allocValue();
    result->setFloat(tempo);
    return result;
}

MslValue* MslStandardLibrary::EndSustain(MslSession* s, MslValue* arguments)
{
    (void)arguments;
    MslSuspendState* state = s->getSustainState();
    state->init();
    return nullptr;
}

MslValue* MslStandardLibrary::EndRepeat(MslSession* s, MslValue* arguments)
{
    (void)arguments;
    MslSuspendState* state = s->getRepeatState();
    state->init();
    return nullptr;
}
