/**
 * Implementations strip elements that are not Parameters
 */

#pragma once

#include "AudioMeter.h"
#include "StripElement.h"
#include "StripRotary.h"

class StripTrackNumber : public StripElement
{
  public:
    
    StripTrackNumber(class TrackStrip* parent);
    ~StripTrackNumber();

    void update(MobiusView* view) override;

    int getPreferredHeight() override;
    int getPreferredWidth() override;
    
    void paint(juce::Graphics& g) override;
    
    void mouseDown(const class juce::MouseEvent& event) override;
    
  private:

    bool focusLock = false;
    UIAction action;
};

class StripGroupName : public StripElement
{
  public:
    
    StripGroupName(class TrackStrip* parent);
    ~StripGroupName();

    void update(MobiusView* view) override;

    int getPreferredHeight() override;
    int getPreferredWidth() override;
    
    void paint(juce::Graphics& g) override;
    
  private:

};

class StripFocusLock : public StripElement
{
  public:
    
    StripFocusLock(class TrackStrip* parent);
    ~StripFocusLock();

    int getPreferredHeight() override;
    int getPreferredWidth() override;

    void update(MobiusView* view) override;
    void paint(juce::Graphics& g) override;
    
    void mouseDown(const class juce::MouseEvent& event) override;

  private:
    bool focusLock = false;
    UIAction action;
};

class StripLoopRadar : public StripElement
{
  public:
    
    StripLoopRadar(class TrackStrip* parent);
    ~StripLoopRadar();

    int getPreferredHeight() override;
    int getPreferredWidth() override;

    void update(MobiusView* view) override;
    void paint(juce::Graphics& g) override;

  private:

    long loopFrames = 0;
    long loopFrame = 0;
    juce::Colour loopColor;
};

class StripLoopThermometer : public StripElement
{
  public:
    
    StripLoopThermometer(class TrackStrip* parent);
    ~StripLoopThermometer();

    int getPreferredHeight() override;
    int getPreferredWidth() override;

    void update(MobiusView* view) override;
    void paint(juce::Graphics& g) override;

  private:

    long loopFrames = 0;
    long loopFrame = 0;

};
    
class StripOutputMeter : public StripElement
{
  public:
    
    StripOutputMeter(class TrackStrip* parent);
    ~StripOutputMeter();

    int getPreferredHeight() override;
    int getPreferredWidth() override;

    void resized() override;
    void update(MobiusView* view) override;

  private:

    AudioMeter meter;
};
    
class StripInputMeter : public StripElement
{
  public:
    
    StripInputMeter(class TrackStrip* parent);
    ~StripInputMeter();

    int getPreferredHeight() override;
    int getPreferredWidth() override;

    void resized() override;
    void update(MobiusView* view) override;

  private:

    AudioMeter meter;
};
    
class StripLoopStack : public StripElement, public juce::FileDragAndDropTarget
{
  public:
    
    StripLoopStack(class TrackStrip* parent);
    ~StripLoopStack();

    // this is the only one right now responsive to configuration
    void configure() override;

    int getPreferredHeight() override;
    int getPreferredWidth() override;

    void update(MobiusView* view) override;
    void paint(juce::Graphics& g) override;
    
    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragMove(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    
  private:

    // maximum number of loops we will display
    int maxLoops = 0;
    // number of loops actually in the track
    int trackLoops = 0;
    // the first loop being displayed
    int firstLoop = 0;
    
    int lastActive = -1;
    int lastDropTarget = -1;
    int dropTarget = -1;
    bool hoverTarget = false;
    
    int getDropTarget(int x, int y);
};
        
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
