/**
 * Simple rectangular bouncing meter to show audio levels.
 * This is a child of two wrapper components:
 *
 *      InputMeterElement
 *        A StatusElement that shows the input level in the active track
 *
 *      StripOutputMeter
 *        A StripElement that shows the output level in the associated track
 *
 * There is no OutputMeterElement or StripInputMeter though those could be added.
 * OG Mobius only had the first two.
 *
 */

#pragma once

class AudioMeter : public juce::Component
{
  public:
    
    AudioMeter();
    virtual ~AudioMeter();

    void update(int value);

    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    int range = 0;
    int savedValue = 0;
    int savedLevel = 0;

};

    
