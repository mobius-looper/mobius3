
#pragma once
#include <JuceHeader.h>


/**
 * Components to display temporary alerts and prompts.
 *
 * juce::AlertWindow is too heavy weight and unreliable for plugins,
 * most people seem to implement "dialogs" with normal Components that
 * can be temporarily displayed over the top of the main UI components.
 *
 * Alerter is a single object managed by Supervisor that can add
 * AlertComponents to the main window component.
 *
 */

class AlertComponent : public juce::Component, juce::Button::Listener
{
  public:

    AlertComponent(class Alerter* alerter, juce::String message);
    ~AlertComponent();

    void resized() override;
    void paint(juce::Graphics& g) override;
    void buttonClicked(juce::Button* b) override;

  private:

    Alerter* alerter = nullptr;
    juce::String text;
    
    juce::Label label;
    juce::TextButton okButton {"Ok"};

    int centerLeft(juce::Component& c);
};


/**
 * Class for managing various kinds of alerts.
 */
class Alerter
{
    friend class AlertComponent;
    
  public:

    Alerter(class Supervisor* s);
    ~Alerter();
    
    void alert(juce::Component* parent, juce::String text);

  protected:

    void closeMe(AlertComponent* alert);

  private:

    void center(juce::Component* parent, juce::Component* child);

    class Supervisor* supervisor = nullptr;

    juce::OwnedArray<AlertComponent> active;
    juce::OwnedArray<AlertComponent> finished;
    
};
