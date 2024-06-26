/**
 * Temporary test for the NewConfigPanel
 */

#pragma once

class TestConfigContent : public juce::Component
{
  public:

    TestConfigContent() {}
    ~TestConfigContent() {}

  private:
};

class TestConfigPanel : public ConfigPanel
{
  public:
    
    TestConfigPanel();
    virtual ~TestConfigPanel();

    void load();
    void save();
    void cancel();

  private:

    TestConfigContent testContent;

};
