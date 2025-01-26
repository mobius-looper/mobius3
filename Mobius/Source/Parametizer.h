/**
 * Class encapsulating management of plugin parameters for Supervisor.
 *
 */

#pragma once

class Parametizer
{
  public:

    Parametizer(class Provider* p);
    ~Parametizer();

    void initialize();
    void install();

    juce::OwnedArray<class PluginParameter>* getParameters() {
        return &parameters;
    }
    
  private:

    class Provider* provider = nullptr;

    juce::OwnedArray<class PluginParameter> parameters;
    
};

