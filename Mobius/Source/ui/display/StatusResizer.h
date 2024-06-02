
#pragma once

class StatusResizer : public juce::ResizableBorderComponent
{
  public:

    StatusResizer(class StatusElement* el);
    ~StatusResizer() {}

    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

  private:

    StatusElement* element = nullptr;
};
