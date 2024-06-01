/**
 * Class encapsulating management of plugin parameters for Supervisor.
 *
 */

#pragma once

class Parametizer
{
  public:

    Parametizer(Supervisor* super);
    ~Parametizer();

    void initialize();
    void install();

    juce::OwnedArray<class PluginParameter>* getParameters() {
        return &parameters;
    }
    
  private:

    class Supervisor* supervisor = nullptr;

    juce::OwnedArray<class PluginParameter> parameters;
    
};

