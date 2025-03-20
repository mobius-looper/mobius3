/**
 * Yer basic lock free ring buffer of pointers.
 * Sure, a template would be better but we're old-school.
 */

#pragma once

class RingBuffer
{
  public:

    // this allocates memory so it must be done in the UI thread
    RingBuffer(juce::String name, int size);
    ~RingBuffer();

    // add a pointer to the buffer
    // returns false if it was full and complains about it
    bool add(void* ptr);

    // return the next pointer in the buffer
    // returns nullptr if it is empty
    void* remove();

  private:

    juce::String name;

    // probably should use a vector here
    std::unique_ptr<void*> buffer;
    
    int bufferSize = 0;
    int head = 0;
    int tail = 0;
};



    
