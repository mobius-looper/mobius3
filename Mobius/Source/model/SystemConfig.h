/**
 * Definitions for system characteristics that are not editable, but are more
 * convenient kept outside the compiled code.
 *
 * This will be stored in the system.xml file.
 *
 * This is a funny one that started as a home for parameter category definitions
 * for the SessionEditor.  Things in here drive parts of the UI but unlike UIConfig,
 * you can't edit them, and are normally only changed on releases.
 *
 * As such it's more like HelpCatalog, and arguably HelpCatalog (and help.xml) could
 * be inside the SystemConfig.  But help is much larger and more convenient to keep
 * outside and in a different file.
 *
 * Other things that could go in here in time are the definitions of the buit-in
 * UIElementDefinitions.
 *
 * Nothing in here currently relates to the engine.
 * 
 */

class SystemConfig
{
  public:

    class Category {
      public:
        juce::String name;
        juce::String formTitle;
    };

    void parseXml(juce::String xml);

    Category* getCategory(juce::String name);

  private:

    juce::OwnedArray<Category> categories;
    juce::HashMap<juce::String,Category*> categoryMap;

    void xmlError(const char* msg, juce::String arg);
    Category* parseCategory(juce::XmlElement* root);

};
