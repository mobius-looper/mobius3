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
    void captureDeviceState(class DeviceConfig* config);
    void traceDeviceSetup();
    
  private:

    class Supervisor* supervisor;
    bool startupError = false;
    
    void openAudioDevice();

};
