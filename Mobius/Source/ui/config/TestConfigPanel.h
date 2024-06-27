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

class TestConfigPanel : public NewConfigPanel
{
  public:
    
    TestConfigPanel(class ConfigEditor* editor);
    virtual ~TestConfigPanel();

    void load();
    void save();
    void cancel();

  private:

    TestConfigContent testContent;

};
