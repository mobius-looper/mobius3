/**
 * An object used to coordinate one track waiting for something in another track.
 * Conceptually similar to MslWait but used only for cross-track synchronization.
 */

#pragma once

#include "../../model/ParameterConstants.h"

class TrackWait
{
  public:

    /**
     * The logical track number of the track that asked for the wait.
     */
    int follower = 0;

    /**
     * The quantization point to wait for
     */
    QuantizeMode quantizationPoint = QUANTIZE_OFF;

    /**
     * Pointer to an information payload, tracking event, or
     * some other unknown state maintained by the requesting track.
     * May also just be a unique id number.
     * Passed back to the requesting track when the wait completes or
     * is canceled.
     */
    void* requestPayload = nullptr;

    /**
     * Pointer to an opaque object maintained by the target track to handle
     * this wait request.  e.g. a scheduled Event or TrackEvent pointer.
     */
    void* responsePayload = nullptr;

    // todo: since this is a bi-directional object could also allow the
    // receiving track to pass back errors here
    
};



