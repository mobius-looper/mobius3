
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "../model/XmlRenderer.h"
#include "../model/MobiusConfig.h"
#include "../model/Preset.h"
#include "../model/Setup.h"
#include "../model/Binding.h"
#include "../Supervisor.h"
#include "UpgradePanel.h"

const int UpgradePanelFooterHeight = 20;

UpgradePanel::UpgradePanel()
{
    okButton.addListener(this);
    loadButton.addListener(this);
    
    addAndMakeVisible(footer);
    footer.addAndMakeVisible(okButton);

    addAndMakeVisible(&loadButton);
    addAndMakeVisible(&log);
    
    setSize(800, 600);
}

UpgradePanel::~UpgradePanel()
{
}

void UpgradePanel::show()
{
    centerInParent();
    setVisible(true);
}

void UpgradePanel::buttonClicked(juce::Button* b)
{
    if (b == &okButton)
      setVisible(false);
    else if (b == &loadButton)
      doUpgrade();
}

void UpgradePanel::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    area.removeFromBottom(5);
    area.removeFromTop(5);
    area.removeFromLeft(5);
    area.removeFromRight(5);

    juce::Rectangle<int> buttonArea = area.removeFromTop(20);
    loadButton.setBounds(buttonArea.getX(), buttonArea.getY(), 100, buttonArea.getHeight());
    
    footer.setBounds(area.removeFromBottom(UpgradePanelFooterHeight));
    okButton.setSize(60, UpgradePanelFooterHeight);

    log.setBounds(area);
    
    centerInParent(okButton);
}

void UpgradePanel::paint(juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    g.setColour(juce::Colours::white);
    g.drawRect(getLocalBounds(), 4);
}

//
// todo: this shit happens a lot, put something in JuceUtil
//

int UpgradePanel::centerLeft(juce::Component& c)
{
    return centerLeft(this, c);
}

int UpgradePanel::centerLeft(juce::Component* container, juce::Component& c)
{
    return (container->getWidth() / 2) - (c.getWidth() / 2);
}

int UpgradePanel::centerTop(juce::Component* container, juce::Component& c)
{
    return (container->getHeight() / 2) - (c.getHeight() / 2);
}

void UpgradePanel::centerInParent(juce::Component& c)
{
    juce::Component* parent = c.getParentComponent();
    c.setTopLeftPosition(centerLeft(parent, c), centerTop(parent, c));
}    

void UpgradePanel::centerInParent()
{
    centerInParent(*this);
}    

//////////////////////////////////////////////////////////////////////
//
// Upgrade
//
//////////////////////////////////////////////////////////////////////

void UpgradePanel::doUpgrade()
{
    doFileChooser();
}

void UpgradePanel::doFileChooser()
{
    juce::File startPath(Supervisor::Instance->getRoot());
    if (lastFolder.length() > 0)
      startPath = lastFolder;
    
    // a form of smart pointer
    chooser = std::make_unique<juce::FileChooser> ("Select an XML file ...",
                                                   startPath,
                                                   "*.xml");

    auto chooserFlags = juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectFiles;
    
    chooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        // magically get here after the modal dialog closes
        // the array will be empty if Cancel was selected
        juce::Array<juce::File> result = fc.getResults();
        if (result.size() > 0) {
            //Trace(2, "FileChooser chose these files:\n");
            for (auto file : result) {

                juce::String path = file.getFullPathName();
                //Trace(2, "  %s\n", path.toUTF8());

                doUpgrade(file);

                // remember this directory for the next time
                lastFolder = file.getParentDirectory().getFullPathName();
            }
        }
        
    });
}

void UpgradePanel::doUpgrade(juce::File file)
{
    log.add("Upgrading: " + file.getFullPathName());
    
    juce::String xml = file.loadFileAsString();
    if (!xml.contains("MobiusConfig")) {
        log.add("File does not contain a MobiusConfig");
    }
    else {
        XmlRenderer xr;
        MobiusConfig* config = xr.parseMobiusConfig(xml.toUTF8());
        if (config == nullptr) {
            log.add("Parser error in file");
        }
        else {
            log.add("MobiusConfig file parsed");
            char* cxml = xr.render(config);
            log.add(cxml);
            delete cxml;

            Preset* preset = config->getPresets();
            while (preset != nullptr) {
                const char* pname = preset->getName();
                juce::String jpname = juce::String(pname);
                juce::String upname = "Upgrade:" + jpname;
                log.add("Upgrading Preset: " + jpname + " -> " + upname);
                preset = preset->getNextPreset();
            }
            
            Setup* setup = config->getSetups();
            while (setup != nullptr) {
                const char* name = setup->getName();
                juce::String jname = juce::String(name);
                juce::String upname = "Upgrade:" + jname;
                log.add("Upgrading Setup: " + jname + " -> " + upname);
                setup = setup->getNextSetup();
            }

            BindingSet* bindings = config->getBindingSets();
            while (bindings != nullptr) {
                const char* name = bindings->getName();
                // these normally do not have names
                if (name == nullptr)
                  name = "unnamed";
                juce::String jname = juce::String(name);
                juce::String upname = "Upgrade:" + jname;
                log.add("Upgrading Bindings: " + jname + " -> " + upname);
                bindings = bindings->getNextBindingSet();
            }

            
        }

        delete config;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
