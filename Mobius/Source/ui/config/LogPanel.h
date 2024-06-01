/*
 * Extension of TextEditor for experimentation.
 */

#pragma once

#include <JuceHeader.h>

class LogPanel : public juce::TextEditor
{
  public:

    LogPanel();
    ~LogPanel();

    void add(const juce::String& m);

  private:

};
