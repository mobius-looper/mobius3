
#include <JuceHeader.h>

#include "Trace.h"

#include "RingBuffer.h"

RingBuffer::RingBuffer(juce::String argName, int size)
{
    name = argName;
    buffer.reset(new void*[size]);
    bufferSize = size;
}

RingBuffer::~RingBuffer()
{
}

bool RingBuffer::add(void* ptr)
{
    bool added = false;
    int next = head + 1;
    if (next >= bufferSize)
      next = 0;

    if (next >= tail)
      Trace(1, "RingBuffer: %s Overflow", name.toUTF8());
    else {
        buffer[head] = ptr;
        head = next;
        added = true;
    }
    return added;
}

void* RingBuffer::remove()
{
    void* ptr = nullptr;
    if (head != tail) {
        ptr = buffer[tail];
        int next = tail + 1;
        if (next >= bufferSize)
          next = 0;
        tail = next;
    }
    return ptr;
}

