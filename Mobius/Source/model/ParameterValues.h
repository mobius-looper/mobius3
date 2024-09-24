/**
 * Objet holding the values of parameters.
 *
 * There will always be one default value, and an optional
 * override value for each track.
 *
 * UPDATE: This is not used, nor will it be...
 */

#pragma once

#include <JuceHeader.h>

/**
 * Holder of a single parameter value.
 */
class ParameterValue {
  public:

    ParameterValue() {}
    ~ParameterValue() {}
    
    /**
     * Parameters almost always have a numeri value.
     * For enumarations and structures, this is the ordinal.
     */
    int number = 0;

    /**
     * A few parameters have string values, mostly the names of things.
     */
    // !! this can dynamically grow and is not thread safe
    // even a static array is not thread safe, think about whether
    // we need this and if we do guard it with a csect
    juce::String string;

    /**
     * A flag indicating that the value is unbound or void.
     */
    bool unbound = true;
};

/**
 * Holder of default and track-specific values.
 * The array will be empty for global variables.
 */
class ParameterValues {
  public:

    ParameterValues() {}
    ~ParameterValues() {}

    ParameterValue value;

    juce::Array<ParameterValue> tracks;

    /**
     * Derive the value of the parameter by looking in the track arrays
     * or falling back to the default value.
     */
    int getInt(int track);
    juce::String getString(int track);

    /**
     * Ensure that the track array has values for the configured
     * number of tracks.  This must be done outside the audio thread and
     * never extended while the audio thread is active for thread safety.
     * Typidally done at startup.
     */
    void configure(int maxTracks);
};


     

