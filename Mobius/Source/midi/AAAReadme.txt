Code in this folder is related to the processing of MIDI data, in particular
beat clocks and transport events.  I'm trying to keep this as general as possible
so it can evolve and be reused by other things besides Mobius.

Most of it is quite old and some predates Mobius.  The ancient C++ style has been
retained for faster porting but should be reconsidered in time.

The MidiEvent class is functionally the same as juce::MidiMessage.  I'll
gradually migrate to the Juce model when there isn't anything better to do.

