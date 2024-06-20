/**
 * Class encapsulating the management of audio devices when
 * Mobius is running as a standalone application.
 *
 */

#pragma once

#include <JuceHeader.h>

class AudioManager
{
  public:
    
    AudioManager(Supervisor* super);
    ~AudioManager();

    void openDevices();
    
    void traceDeviceSetup();

    void captureDeviceState();
    
  private:

    class Supervisor* supervisor;

    void openApplicationAudio();
    void openDevicesNew();

};
