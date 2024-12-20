
#include <JuceHeader.h>

#include "MslValue.h"
#include "MslEnvironment.h"

#include "MslStandardLibrary.h"

bool MslStandardLibrary::RandSeeded = false;

MslLibraryDefinition MslLibraryDefinitions[] = {

    {"Time", MslFuncTime},
    {"Rand", MslFuncRand},
    
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

MslValue* MslStandardLibrary::call(MslEnvironment* env, MslLibraryId id, MslValue* arguments)
{
    MslValue* result = nullptr;

    switch (id) {
        case MslFuncTime: result = Time(env, arguments) ; break;
        case MslFuncRand: result = Rand(env, arguments) ; break;
    }

    return result;
}

MslValue* MslStandardLibrary::Time(MslEnvironment* env, MslValue* arguments)
{
    (void)arguments;
    MslValue* v = env->allocValue();
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
 */
MslValue* MslStandardLibrary::Rand(MslEnvironment* env, MslValue* arguments)
{
    // todo: contention on this since it's shared with the host
    if (!RandSeeded) {
        srand((int)(time(nullptr)));
        RandSeeded = true;
    }

    int low = 0;
    int high = 127;
    int value = 0;

    if (arguments != nullptr) {
        MslValue* second = arguments->next;
        if (second == nullptr)
          high = arguments->getInt();
        else {
            low = arguments->getInt();
            high = second->getInt();
        }
    }
    
    if (low >= high) {
        value = low;
    }
    else {
        int range = high - low + 1;
        value = (rand() % range) + low;
    }

    MslValue* v = env->allocValue();
    v->setInt(value);
    return v;
}
