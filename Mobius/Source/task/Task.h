/**
 * Common base class for system tasks.
 *
 * A task can be used for a number of things, but is essentially a sequence
 * of steps that are performed in an order, with some of those steps requiring
 * asynchronous user interaction.
 *
 * It is a new Mobius component that has overlap with a number of other older things
 * that will eventually be redesigned to become tasks.
 *
 */

#pragma once

#include <JuceHeader.h>

class Task
{
  public:

    typedef enum {
        
        None,
        Alert,
        ProjectExport,
        ProjectImport
        
    } Type;
    
    Task();
    virtual ~Task();

    Type getType();
    const char* getTypeName();

    void setId(int i);
    int getId();

    void launch(class Provider* p);
    bool isFinished();

    virtual void run() = 0;

    virtual bool isConcurrent() {
        return false;
    }

    void addError(juce::String e);
    void addWarning(juce::String e);
    bool hasErrors();
    
  protected:

    class Provider* provider = nullptr;
    Type type = None;
    int id = 0;
    bool finished = false;

    juce::StringArray errors;
    juce::StringArray warnings;
    
};
