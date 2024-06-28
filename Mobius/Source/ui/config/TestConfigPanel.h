/**
 * Temporary test for the NewConfigPanel
 */

#pragma once

#include "NewConfigPanel.h"

class TestConfigContent : public ConfigPanelContent, public NewObjectSelector::Listener
{
  public:

    TestConfigContent();
    ~TestConfigContent();

    // ConfigPanelContent
    void showing() override;
    void hiding() override;
    void load() override;
    void save() override;
    void cancel() override;
    void revert() override;
    
    // ObjectSelector::Listener
    void objectSelectorSelect(int ordinal) override;
    void objectSelectorRename(juce::String newName) override;
    void objectSelectorNew() override;
    void objectSelectorDelete() override;
    void objectSelectorCopy() override;
    
  private:
};

class TestConfigPanel : public NewConfigPanel
{
  public:
    
    TestConfigPanel() {
        setName("TestConfigPanel");

        // this goes all the way back to BasePanel
        setTitle("Test Config");
        
        // tell NewConfigPanel we want an object selector and
        // our inner content is the listener
        // do this here or when inner is prepped?
        enableObjectSelector(&testContent);
        
        // we want a help area
        setHelpHeight(10);

        // this is the meat
        setConfigContent(&testContent);

        // NewConfigPanel will already have adjusted the footer buttons of
        // BasePanel to add Save/Canel
        // here we could add more, or defer that to our inner content?
    }
    
    ~TestConfigPanel() {
    }

  private:

    TestConfigContent testContent;

};
