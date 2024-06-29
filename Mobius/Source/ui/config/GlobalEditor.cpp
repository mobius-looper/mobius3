
#include <JuceHeader.h>

#include "../../model/MobiusConfig.h"
#include "../../model/DeviceConfig.h"
#include "../../model/UIParameter.h"
#include "../../Supervisor.h"

#include "ParameterField.h"
#include "GlobalEditor.h"


GlobalEditor::GlobalEditor()
{
    setName("GlobalEditor");
    render();
}

GlobalEditor::~GlobalEditor()
{
}

void GlobalEditor::resized()
{
    form.setBounds(getLocalBounds());
}

void GlobalEditor::prepare()
{
    form.setHelpArea(context->getHelpArea());
}

void GlobalEditor::load()
{
    MobiusConfig* config = context->getMobiusConfig();
    loadGlobal(config);
}

void GlobalEditor::save()
{
    MobiusConfig* config = context->getMobiusConfig();
    saveGlobal(config);
    context->saveMobiusConfig();

    DeviceConfig* dc = context->getDeviceConfig();

    // actually don't need to display the standalone ports the way it's working now
    // it will auto-adjust to whatever ports were selected in AudioDevicesPanel
    //MachineConfig* mc = dc->getMachineConfig();
    //mc->inputPorts = getPortValue(&asioInputs, 32);
    //mc->outputPorts = getPortValue(&asioOutputs, 32);
    dc->pluginConfig.defaultAuxInputs = getPortValue(&pluginInputs, 8) - 1;
    dc->pluginConfig.defaultAuxOutputs = getPortValue(&pluginOutputs, 8) - 1;

    // these auto-save on shutdown, but might want to do it early?
}

/**
 * Extract the value of one of the port fields as an integer
 * and constraint the value.
 */
int GlobalEditor::getPortValue(BasicInput* field, int max)
{
    int value = field->getInt();
    if (value < 1)
      value = 1;
    else if (max > 0 && value > max)
      value = max;
    return value;
}

void GlobalEditor::cancel()
{
    // the copy is inside the form which will be cleared
    // on the next load()
}

/**
 * Load the global config into the parameter fields
 */
void GlobalEditor::loadGlobal(MobiusConfig* config)
{
    juce::Array<Field*> fields;
    form.gatherFields(fields);
    for (int i = 0 ; i < fields.size() ; i++) {
        Field* f = fields[i];
        ParameterField* pf = dynamic_cast<ParameterField*>(f);
        if (pf != nullptr)
          pf->loadValue(config);
    }

    // ports don't come from MobiusConfig
    DeviceConfig* dc = Supervisor::Instance->getDeviceConfig();

    // actually don't need to expose these, we auto-adjust whenever
    // AudioDevicesPanel selects channels
    // MachineConfig* mc = dc->getMachineConfig();
    //asioInputs.setText(juce::String(mc->inputPorts));
    //asioOutputs.setText(juce::String(mc->outputPorts));

    pluginInputs.setText(juce::String(dc->pluginConfig.defaultAuxInputs + 1));
    pluginOutputs.setText(juce::String(dc->pluginConfig.defaultAuxOutputs + 1));

    if (ccThreshold != nullptr) {
        // zero means max, but to make it look right to the user for the
        // first time, bump it up
        int value = config->mControllerActionThreshold;
        if (value == 0) value = 127;
        ccThreshold->setValue(juce::var(value));
    }
}

/**
 * Save the fields back into the master config.
 */
void GlobalEditor::saveGlobal(MobiusConfig* config)
{
    juce::Array<Field*> fields;
    form.gatherFields(fields);
    for (int i = 0 ; i < fields.size() ; i++) {
        Field* f = fields[i];
        ParameterField* pf = dynamic_cast<ParameterField*>(f);
        if (pf != nullptr)
          pf->saveValue(config);
    }

    if (ccThreshold != nullptr) {
        config->mControllerActionThreshold = ccThreshold->getIntValue();
    }
    
}

//////////////////////////////////////////////////////////////////////
//
// Rendering
//
//////////////////////////////////////////////////////////////////////

void GlobalEditor::render()
{
    initForm();
    form.render();

    // after adding the tabs for the form, add one
    // for the random properties that are not in the form
    form.addTab("IO Ports", &properties);
    properties.setLabelColor(juce::Colours::orange);
    properties.setLabelCharWidth(15);
    properties.setTopInset(12);
    //properties.add(&asioInputs);
    //properties.add(&asioOutputs);
    properties.add(&pluginInputs);
    properties.add(&pluginOutputs);
    
    // place it in the content panel
    addAndMakeVisible(form);
}

/**
 * These parameters show in the old dialog but are no longer needed.
 *  Custom Message File, CustomMessageFileParameter
 *  Message Duration/MessageDurationParameter
 *   this is actually a UIType, not in MobiusConfig
 *  Dual Plugin Edit Window
 *
 * These are in MobiusConfig but are edited in dedicated panels
 *   AudioInputParameter
 *   AudioOutputParameter
 *   MidiInputParameter
 *   MidiOutputParameter
 *
 * See parameter-shit for others that are defined but obscure.
 * 
 */
void GlobalEditor::initForm()
{
    // note that while activeSetup is a global parameter we defined
    // it in the SetupPanel as a side effect of leaving it selected?
    // do we?
    // what about DefaultPreset shouldn't that be the same?
    //addField("Miscellaneous", UIParameterDefaultPreset);

    // These are the most useful
    addField("General", UIParameterTrackCount);
    addField("General", UIParameterGroupCount);

    // this doesn't actually do anything in core code
    // loops-per-track has been set in the Preset which
    // isn't ideal
    //addField("General", UIParameterMaxLoops);
    
    addField("General", UIParameterQuickSave);
    addField("General", UIParameterLongPress);
    addField("General", UIParameterAutoFeedbackReduction);

    // this one doesn't have a UIParameter definition, wing it
    ccThreshold = new Field("Controller Action Threshold", Field::Type::Integer);
    form.add(ccThreshold);

    // these are obscure
    addField("Advanced", UIParameterInputLatency);
    addField("Advanced", UIParameterOutputLatency);
    addField("Advanced", UIParameterMaxSyncDrift);
    addField("Advanced", UIParameterNoiseFloor);
    addField("Advanced", UIParameterTraceLevel);
    addField("Advanced", UIParameterSaveLayers);
    addField("Advanced", UIParameterMonitorAudio);

    // support lost, could be useful
    //addField("Advanced", UIParameterSpreadRange);
    
    // support for this got lost
    // might be useful to resurrect
    //addField("General", UIParameterIntegerWaveFile);

    // this one is obscure and either needs to be removed
    // or implemented properly, new Actionator doesn't handle it
    //addField("Miscellaneous", UIParameterGroupFocusLock);

    // these exist as booleans in the model, but I'm doing export a different way now
    //addField("Miscellaneous", UIParameterMidiExport);
    //addField("Miscellaneous", UIParameterHostMidiExport);

    // these are StringList and need rework
    //addField("Functions", UIParameterFocusLockFunctions);
    //addField("Functions", UIParameterMuteCancelFunctions);
    //addField("Functions", UIParameterConfirmationFunctions);
    // addField("Modes", UIParameterAltFeedbackDisable);
}

void GlobalEditor::addField(const char* tab, UIParameter* p)
{
    form.add(new ParameterField(p), tab, 0);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

