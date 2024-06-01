/**
 * Wrapper around Binderator for mapping MIDI events received
 * in the plugin audio thread into actions.
 *
 * todo: this didn't end up doing much compared to ApplicationBindertor
 * could just have MobiusKernel use a Binderator directly.
 */

#include <JuceHeader.h>

#include "../Binderator.h"

#include "MobiusKernel.h"
#include "KernelBinderator.h"

KernelBinderator::KernelBinderator(MobiusKernel* k)
{
    kernel = k;
}

KernelBinderator::~KernelBinderator()
{
    // assuming that if we're destructing we're in a safe
    // context to do memory allocation
    delete binderator;
}

/**
 * Swap a previously constructed Binderator with the one
 * we have been using.
 */
Binderator* KernelBinderator::install(Binderator* b)
{
    Binderator* old = binderator;
    binderator = b;
    return old;
}

UIAction* KernelBinderator::getMidiAction(juce::MidiMessage& msg)
{
    UIAction* action = nullptr;
    if (binderator != nullptr)
      action = binderator->handleMidiEvent(msg);
    return action;
}

