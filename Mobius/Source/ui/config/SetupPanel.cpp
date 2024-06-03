/**
 * Panel for editing setups.
 *
 * TODO: 90% of this is identical to PresetPanel except for the
 * class names and the way the Form is built.  The one complication
 * is that setups are actually two object classes, the outer Setup and the
 * SetupTrack children.
 *
 * Apart from how form fields work, the code could be mostly refactored
 * into a common superclass that manages any Structure lists with virtual
 * methods to handle the differences.  It's not so bad since we only have two
 * right now, but if you ever add another, do this.
 *
 */

#include <JuceHeader.h>

#include <string>
#include <sstream>

#include "../../util/Trace.h"
#include "../../model/UIParameter.h"
#include "../../model/MobiusConfig.h"
#include "../../model/Setup.h"
#include "../../model/XmlRenderer.h"

#include "../common/SimpleRadio.h"

#include "ConfigEditor.h"
#include "ParameterField.h"
#include "SetupPanel.h"

SetupPanel::SetupPanel(ConfigEditor* argEditor) :
    ConfigPanel{argEditor, "Track Setups",
      ConfigPanelButton::Save | ConfigPanelButton::Revert | ConfigPanelButton::Cancel, true}
{
    setName("SetupPanel");
    render();
}

SetupPanel::~SetupPanel()
{
}

//////////////////////////////////////////////////////////////////////
//
// ConfigPanel overloads
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by ConfigEditor when asked to edit Setups.
 */
void SetupPanel::load()
{
    if (!loaded) {
        // build a list of names for the object selector
        juce::Array<juce::String> names;
        // clone the Setup list into a local copy
        setups.clear();
        revertSetups.clear();
        MobiusConfig* config = editor->getMobiusConfig();
        if (config != nullptr) {
            // convert the linked list to an OwnedArray
            Setup* plist = config->getSetups();
            while (plist != nullptr) {
                Setup* s = new Setup(plist);
                setups.add(s);
                names.add(juce::String(plist->getName()));
                // also a copy for the revert list
                revertSetups.add(new Setup(plist));
                
                plist = (Setup*)(plist->getNext());
            }
        }
        
        // this will also auto-select the first one
        objectSelector.setObjectNames(names);

        // load the first one, do we need to bootstrap one if
        // we had an empty config?
        selectedSetup = 0;
        selectedTrack = 0;
        loadSetup(selectedSetup);

        loaded = true;
        // force this true for testing
        changed = true;
    }
}

/**
 * Called by the Save button in the footer.
 * 
 * Save all setups that have been edited during this session
 * back to the master configuration.
 *
 * Tell the ConfigEditor we are done.
 */
void SetupPanel::save()
{
    if (changed) {
        // copy visible state back into the object
        // need to also do this when the selected setup is changed
        saveSetup(selectedSetup);
        
        // build a new linked list
        Setup* plist = nullptr;
        Setup* last = nullptr;
        
        for (int i = 0 ; i < setups.size() ; i++) {
            Setup* s = setups[i];
            if (last == nullptr)
              plist = s;
            else
              last->setNext(s);
            last = s;
        }

        // we took ownership of the objects so
        // clear the owned array but don't delete them
        setups.clear(false);

        MobiusConfig* config = editor->getMobiusConfig();
        // this will also delete the current setup list
        config->setSetups(plist);
        editor->saveMobiusConfig();

        loaded = false;
        changed = false;
    }
    else if (loaded) {
        // throw away setup copies
        setups.clear();
        loaded = false;
    }
}

/**
 * Throw away all editing state.
 */
void SetupPanel::cancel()
{
    // delete the copied setups
    setups.clear();
    revertSetups.clear();
    loaded = false;
    changed = false;
}

//////////////////////////////////////////////////////////////////////
//
// ObjectSelector overloads
// 
//////////////////////////////////////////////////////////////////////

/**
 * Called when the combobox changes.
 */
void SetupPanel::selectObject(int ordinal)
{
    if (ordinal != selectedSetup) {
        saveSetup(selectedSetup);
        selectedSetup = ordinal;
        loadSetup(selectedSetup);
    }
}

void SetupPanel::newObject()
{
    int newOrdinal = setups.size();
    Setup* neu = new Setup();
    neu->setName("[New]");

    // copy the current setup into the new one?
    // todo: more work on copying
    //neu->copy(setups[selectedSetup]);

    setups.add(neu);
    // make another copy for revert
    Setup* revert = new Setup(neu);
    revertSetups.add(revert);
    
    objectSelector.addObjectName(juce::String(neu->getName()));
    // select the one we just added
    objectSelector.setSelectedObject(newOrdinal);
    selectedSetup = newOrdinal;
    loadSetup(selectedSetup);
}

/**
 * Delete is somewhat complicated.
 * You can't undo it unless we save it somewhere.
 * An alert would be nice, ConfigPanel could do that.
 */
void SetupPanel:: deleteObject()
{
    if (setups.size() == 1) {
        // must have at least one setup
    }
    else {
        setups.remove(selectedSetup);
        revertSetups.remove(selectedSetup);
        // leave the index where it was and show the next one,
        // if we were at the end, move back
        int newOrdinal = selectedSetup;
        if (newOrdinal >= setups.size())
          newOrdinal = setups.size() - 1;
        selectedSetup = newOrdinal;
        loadSetup(selectedSetup);
    }
}

void SetupPanel::revertObject()
{
    Setup* revert = revertSetups[selectedSetup];
    if (revert != nullptr) {
        Setup* reverted = new Setup(revert);
        setups.set(selectedSetup, reverted);
        // what about the name?
        loadSetup(selectedSetup);
    }
}

void SetupPanel::renameObject(juce::String newName)
{
}

//////////////////////////////////////////////////////////////////////
//
// Internal Methods
// 
//////////////////////////////////////////////////////////////////////

/**
 * Load a setup into the parameter fields
 */
void SetupPanel::loadSetup(int index)
{
    Setup* setup = setups[index];
    if (setup != nullptr) {
        juce::Array<Field*> fields;
        form.gatherFields(fields);
        for (int i = 0 ; i < fields.size() ; i++) {
            Field* f = fields[i];
            ParameterField* pf = dynamic_cast<ParameterField*>(f);
            if (pf != nullptr) {
                UIParameter* p = pf->getParameter();
                if (p->scope == ScopeSetup) {
                    pf->loadValue(setup);
                }
                else if (p->scope == ScopeTrack) {
                    SetupTrack* track = setup->getTrack(selectedTrack);
                    pf->loadValue(track);
                }
            }
        }
    }
}

/**
 * Save one of the edited setups back to the master config
 * Think...should save/cancel apply to the entire list of setups
 * or only the one currently being edited.  I think it would be confusing
 * to keep an editing transaction over the entire list
 * When a setup is selected, it should throw away changes that
 * are in progress to the current setup.
 */
void SetupPanel::saveSetup(int index)
{
    Setup* setup = setups[index];
    if (setup != nullptr) {
        juce::Array<Field*> fields;
        form.gatherFields(fields);
        for (int i = 0 ; i < fields.size() ; i++) {
            Field* f = fields[i];
            ParameterField* pf = dynamic_cast<ParameterField*>(f);
            if (pf != nullptr) {
                UIParameter* p = pf->getParameter();
                if (p->scope == ScopeSetup) {
                    pf->saveValue(setup);
                }
                else if (p->scope == ScopeTrack) {
                    SetupTrack* track = setup->getTrack(selectedTrack);
                    pf->saveValue(track);
                }
            }
        }
    }
}

Setup* SetupPanel::getSelectedSetup()
{
    Setup* setup = nullptr;
    if (setups.size() > 0) {
        if (selectedSetup < 0 || selectedSetup >= setups.size()) {
            // shouldn't happen, default back to first
            selectedSetup = 0;
        }
        setup = setups[selectedSetup];
    }
    return setup;
}

//////////////////////////////////////////////////////////////////////
//
// Form Rendering
//
//////////////////////////////////////////////////////////////////////

void SetupPanel::render()
{
    initForm();
    form.render();

    // Track panel is special
    trackSelector = new SimpleRadio();
    int ntracks = 8; // TODO: need to get this from config
    juce::StringArray trackNumbers;
    for (int i = 0 ; i < ntracks ; i++) {
        trackNumbers.add(juce::String(i+1));
    }
    trackSelector->setButtonLabels(trackNumbers);
    trackSelector->setLabel("Track");
    trackSelector->setSelection(0);
    trackSelector->setListener(this);
    trackSelector->render();

    // this will take ownership of the Component
    FormPanel* formPanel = form.getPanel("Tracks");
    formPanel->addHeader(trackSelector);
    
    initButton = new SimpleButton("Initialize");
    initButton->addListener(this);
    initAllButton = new SimpleButton("Initialize All");
    initAllButton->addListener(this);
    
    captureButton = new SimpleButton("Capture");
    captureButton->addListener(this);
    captureAllButton = new SimpleButton("Capture All");
    captureAllButton->addListener(this);

    Panel* buttons = new Panel(Panel::Orientation::Horizontal);
    buttons->addOwned(initButton);
    buttons->addOwned(initAllButton);
    buttons->addOwned(captureButton);
    buttons->addOwned(captureAllButton);
    buttons->autoSize();
    // would be nice to have a panel option that adjusts the
    // width of all the buttons to be the same
    // buttons->setUniformWidth(true);
    formPanel->addFooter(buttons);

    // place it in the content panel
    content.addAndMakeVisible(form);

    // at this point the component hierarhcy has been fully constructed
    // but not sized, until we support bottom up sizing start with
    // a fixed size, this will cascade resized() down the child hierarchy

    // until we get auto-sizing worked out, make this plenty wide
    // MainComponent is currently 1000x1000
    setSize (900, 600);
}

void SetupPanel::initForm()
{
    form.setHelpArea(&helpArea);
    
    // missing:
    // resetRetains, overlayBindings

    addField("Tracks", UIParameterTrackName);
    addField("Tracks", UIParameterSyncSource);    // overrides default SyncSourceParameter
    addField("Tracks", UIParameterTrackSyncUnit);
    addField("Tracks", UIParameterStartingPreset);
    addField("Tracks", UIParameterGroup);
    addField("Tracks", UIParameterFocus);
    addField("Tracks", UIParameterInput);
    addField("Tracks", UIParameterOutput);
    addField("Tracks", UIParameterFeedback);
    addField("Tracks", UIParameterAltFeedback);
    addField("Tracks", UIParameterPan);
    addField("Tracks", UIParameterMono);

    // these were arranged on a 4x4 sub-grid
    addField("Tracks", UIParameterAudioInputPort);
    addField("Tracks", UIParameterAudioOutputPort);
    addField("Tracks", UIParameterPluginInputPort);
    addField("Tracks", UIParameterPluginOutputPort);

    addField("Synchronization", UIParameterDefaultSyncSource);
    addField("Synchronization", UIParameterDefaultTrackSyncUnit);
    addField("Synchronization", UIParameterSlaveSyncUnit);
    addField("Synchronization", UIParameterBeatsPerBar);
    addField("Synchronization", UIParameterRealignTime);
    addField("Synchronization", UIParameterOutRealign);
    addField("Synchronization", UIParameterMuteSyncMode);
    addField("Synchronization", UIParameterResizeSyncAdjust);
    addField("Synchronization", UIParameterSpeedSyncAdjust);
    addField("Synchronization", UIParameterMinTempo);
    addField("Synchronization", UIParameterMaxTempo);
    addField("Synchronization", UIParameterManualStart);

    // Other
    // this needs to be done in a more obvious way

    addField("Other", UIParameterActiveTrack);

    // this one has special values
    //form.add(buildResetablesField(), "Other");

    // Binding Overlay
}

void SetupPanel::addField(const char* tab, UIParameter* p)
{
    form.add(new ParameterField(p), tab, 0);
}

#if 0
Field* SetupPanel::buildResetablesField()
{
    Field* field = new ParameterField(UIParameterResetables);
    juce::StringArray values;
    juce::StringArray displayValues;
    
    // values are defined by Parameter flags
	for (int i = 0 ; i < UIParameter::Parameters.size() ; i++) {
        UIParameter* p = UIParameter::Parameters[i];
        if (p->resettable) {
            values.add(p->getName());
            displayValues.add(p->getDisplayName());
        }
    }

    // Mobius sorted the displayName list, should do the same!

    field->setAllowedValues(values);
    field->setAllowedValueLabels(displayValues);

    return field;
}
#endif

//////////////////////////////////////////////////////////////////////
//
// Listneners
//
//////////////////////////////////////////////////////////////////////

/**
 * Respond to the track selection radio
 */
void SetupPanel::radioSelected(SimpleRadio* r, int index)
{
    (void)r;
    // we don't have saveTrack, so just capture the entire form
    // when changing tracks
    saveSetup(selectedSetup);
    selectedTrack = index;
    loadSetup(selectedSetup);
}

/**
 * Respond to one of the init or capture buttons
 */
void SetupPanel::buttonClicked(juce::Button* b)
{
    Trace(1, "Button %s\n", b->getButtonText().toUTF8());
}

