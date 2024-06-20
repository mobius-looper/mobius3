
#pragma once

#include "common/LogPanel.h"
#include "BasePanel.h"

class EnvironmentContent : public juce::Component
{
  public:

    EnvironmentContent();
    ~EnvironmentContent();

    void resized() override;
    
    void showing();
    
    LogPanel log;
    
  private:

    juce::String getListOfActiveBits (const juce::BigInteger& b);

};

class EnvironmentPanel : public BasePanel
{
  public:

    EnvironmentPanel();
    ~EnvironmentPanel();

    void showing() override;
    void hiding() override;
    
  private:

    EnvironmentContent content;
};

    
