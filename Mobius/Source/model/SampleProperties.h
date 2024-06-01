/**
 * Structure that is attached to a Symbol associated with
 * a Sample.  See ScriptProperties for more on the general notion
 * of behavior properties attached to symbols.
 *
 * Since samples do not have complex internal structure that is "parsed"
 * like scripts, what is here is mostly just a copy of what was in
 * the SampleConfig.
 */

#pragma once

class SampleProperties
{
  public:

    SampleProperties() {}
    ~SampleProperties() {}

    /**
     * True if this script should be automatically given an action
     * button in the main display.  This was just a test hack I liked
     * but may be of general interest.
     */
    bool button = false;

    /**
     * Handle to the internal object that plays this sample.
     */
    void* coreSamplePlayer = nullptr;

    /**
     * The internal numeric index of this sample.
     * This can be used as a binding argument for the PlaySample function.
     * todo: if this works out, then we don't need to hang the coreSamplePlayer
     * here since we can get to it fast enough with the index.
     */
    int index = -1;

    
};

