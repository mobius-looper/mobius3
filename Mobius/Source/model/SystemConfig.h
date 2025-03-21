/**
 * Definitions for a few mutable system properties that live outside the Session.
 *
 * These are not as of yet actionable parameters.
 *
 * todo: devices.xml fits into this category, consider moving DeviceConfig inside
 * this one.
 *
 * Among the things that could go in here:
 *
 *    alternative locations for user defined content: sessions, scripts, samples
 * 
 */

#pragma once

#include "ValueSet.h"
#include "BindingSets.h"
#include "GroupDefinition.h"

class SystemConfig
{
  public:

    constexpr static const char* XmlElementName = "SystemConfig";

    constexpr static const char* StartupSession = "startupSession";
    constexpr static const char* QuicksaveFile = "quicksaveFile";
    constexpr static const char* UserFileFolder = "userFileFolder";

    void parseXml(juce::XmlElement* root, juce::StringArray& errors);
    juce::String toXml();

    /**
     * The name of the session that is considered to be the Startup Session.
     * If unspecified it will auto-generate an empty session.
     */
    juce::String getStartupSession();
    void setStartupSession(juce::String s);

    ValueSet* getValues();
    const char* getString(juce::String name);
    int getInt(juce::String name);

    class BindingSets* getBindings();
    void setBindings(class BindingSets* sets);
    
    class GroupDefinitions* getGroups();
    void setGroups(class GroupDefinitions* groups);
    
  private:

    ValueSet values;
    std::unique_ptr<class BindingSets> bindings;
    std::unique_ptr<class GroupDefinitions> groups;
};
