/**
 * Subcomponent of SessionEditor for editing each track configuration.
 */

#pragma once

#include <JuceHeader.h>

class SessionTrackEditor : public juce::Component
{
  public:

    SessionTrackEditor(class Provider* p);
    ~SessionTrackEditor();

    void loadSymbols();
    void load();
    
    void resized() override;
    
  private:

    class Provider* provider = nullptr;

    std::unique_ptr<class SessionTrackTable> tracks;
    std::unique_ptr<class SessionTrackTrees> trees;
};

        
