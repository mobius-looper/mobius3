/**
 * Combines a set of RingBuffers and SimpleObjectPools to manage
 * messages passed between two threads.
 *
 * There are expected to be two threads involved.  A "shell" thread
 * that is allowed to allocate memory and a "kernel" thread that is not.
 * In practce the shell thread will be the main UI thread or a
 * maintenance thread.  If both a UI and maintenance thread exist, it is expected
 * that the containing environment will organize blocking around those
 * so that they both can't be sending or receiving kernel messsages at the
 * same time.
 */

#pragma once

#include <JuceHeader.h>

#include "RingBuffer.h"
#include "SimpleObjectPool.h"

/**
 * This is expected to be subclassed.
 * The only reason for this is the "consumed" flag which is used
 * to pass messages back to the other side after they have been
 * processed and need to be returned to the originating pool.
 */
class ThreadMessage : public SimplePooledObject
{
  public:

    void setConsumed(bool b) {
        consumed = b;
    }

    bool isConsumed() {
        return consumed;
    }

  private:

    bool consumed = false;
};

class ThreadCommunicator
{
  public:

  private:

    RingBuffer toShell("Shell", 128);
    RingBuffer toKernel("Kernel", 128);
    
