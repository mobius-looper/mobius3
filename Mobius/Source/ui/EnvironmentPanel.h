
#pragma once

#include "common/LogPanel.h"
#include "BasePanel.h"

class EnvironmentContent : public juce::Component
{
  public:

    EnvironmentContent(class Supervisor* s);
    ~EnvironmentContent();

    void resized() override;
    
    void showing();
    
    LogPanel log;
    
  private:

    class Supervisor* supervisor = nullptr;

    juce::String getListOfActiveBits (const juce::BigInteger& b);

};

class EnvironmentPanel : public BasePanel
{
  public:

    EnvironmentPanel(Supervisor* s);
    ~EnvironmentPanel();

    void showing() override;
    
  private:
    EnvironmentContent content;
};

    
