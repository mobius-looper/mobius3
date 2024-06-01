/*
 * New and evolving utility to send Trace messages over the network
 * to a server that can display them.
 *
 * There can only be one fo these.
 */

#pragma once

#include <JuceHeader.h>

class TraceClient
{
  public:

    TraceClient();
    ~TraceClient();

    void setEnabled(bool b);

    void enable() {
        setEnabled(true);
    }
    void disable() {
        setEnabled(false);
    }
    
    void send(const char* msg);

  private:
    
    bool enabled = false;
    bool connectionFailed = false;
    
    juce::StreamingSocket *socket = nullptr;

    void connect();
    void disconnect();
    
};

extern TraceClient TraceClient;

    
 
  
