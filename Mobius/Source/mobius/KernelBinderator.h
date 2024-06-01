/**
 * Wrapper around Binderator for mapping MIDI events received
 * in the plugin audio thread into actions.
 */

#pragma once

class KernelBinderator
{
  public:

    KernelBinderator(class MobiusKernel* kernel);
    ~KernelBinderator();

    class Binderator* install(class Binderator* b);

    class UIAction* getMidiAction(juce::MidiMessage& msg);

  private:

    class MobiusKernel* kernel;

    // unlike ApplicationBinderator this has to be
    // built and passed down whenever it changes so we use
    // a pointer rather than a static member
    class Binderator* binderator = nullptr;

};
