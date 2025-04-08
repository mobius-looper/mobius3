/**
 * ConfigEditor for editing the SystemConfig object.
 */

#include <JuceHeader.h>

#include "../../Supervisor.h"
#include "../../model/SystemConfig.h"
#include "../../model/StaticConfig.h"
#include "../../model/DeviceConfig.h"

#include "SystemEditor.h"

SystemEditor::SystemEditor(Supervisor* s) : ConfigEditor(s)
{
    setName("SystemEditor");

    tabs.add("Plugin", &plugin);
    tabs.add("Files", &files);
                 
    addAndMakeVisible(&tabs);
}

SystemEditor::~SystemEditor()
{
}

void SystemEditor::load()
{
    SystemConfig* config = supervisor->getSystemConfig();
    ValueSet* master = config->getValues();
    values.reset(new ValueSet(master));
    
    // data driven form definition is more complex than I like
    // for the constructor, do it on the first load
    initForm(plugin, "systemPlugin");
    initForm(files, "systemFiles");

    // plugin ports are not actually in system.xml, pretend
    loadPluginValues();
    
    plugin.load(values.get());
    files.load(values.get());
}

void SystemEditor::loadPluginValues()
{
    // these are in the DeviceConfig but since there is no UI for that
    // show them as if they were session globals
    // hacky, needs thought
    DeviceConfig* dc = supervisor->getDeviceConfig();
    // the +1 is because the value is actually the number of "aux" pins and
    // there is always 1 "main" pin, but to the user it looks like they're combined
    values->setInt("pluginInputs", dc->pluginConfig.defaultAuxInputs + 1);
    values->setInt("pluginOutputs", dc->pluginConfig.defaultAuxOutputs + 1);
}

void SystemEditor::initForm(ValueSetForm& form, const char* defname)
{
    if (form.isEmpty()) {
        StaticConfig* scon = supervisor->getStaticConfig();
        Form* formdef = scon->getForm(defname);
        if (formdef == nullptr) {
            Trace(1, "SystemEditor: Missing form definition %s", defname);
        }
        else {
            form.build(supervisor, formdef);
        }
    }
}

void SystemEditor::save()
{
    SystemConfig* config = supervisor->getSystemConfig();
    ValueSet* master = config->getValues();

    files.save(master);
    // this one is weird
    plugin.save(values.get());
    savePluginValues();
    
    supervisor->updateSystemConfig();
}

void SystemEditor::savePluginValues()
{
    // reverse the silly plugin pins thing
    // note that we have to get this from the master since that's
    // where we just committed the form changes
    int newPluginInputs = getPortValue("pluginInputs", 8) - 1;
    int newPluginOutputs = getPortValue("pluginOutputs", 8) - 1;

    DeviceConfig* dc = supervisor->getDeviceConfig();
    dc->pluginConfig.defaultAuxInputs = newPluginInputs;
    dc->pluginConfig.defaultAuxOutputs = newPluginOutputs;
    supervisor->updateDeviceConfig();
}

int SystemEditor::getPortValue(const char* name, int max)
{
    int value = values->getInt(name);
    if (value < 1)
      value = 1;
    else if (max > 0 && value > max)
      value = max;
    return value;
}

void SystemEditor::cancel()
{
    values.reset();
}

void SystemEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    tabs.setBounds(area);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

