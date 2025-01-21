/**
 * ConfigEditor for editing setups.
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

#include "../../util/Trace.h"
#include "../../model/UIParameter.h"
#include "../../model/MobiusConfig.h"
#include "../../model/Setup.h"
#include "../../model/XmlRenderer.h"
#include "../../Supervisor.h"

#include "../common/SimpleRadio.h"

#include "ParameterField.h"
#include "SetupEditor.h"

SetupEditor::SetupEditor(Supervisor* s) : ConfigEditor(s)
{
    setName("SetupEditor");
    render();
}

SetupEditor::~SetupEditor()
{
}

void SetupEditor::prepare()
{
    context->enableObjectSelector();
    // here or in the constructor?
    //render();
    form.setHelpArea(context->getHelpArea());
}

void SetupEditor::resized()
{
    form.setBounds(getLocalBounds());
}

//////////////////////////////////////////////////////////////////////
//
// ConfigEditor overloads
//
//////////////////////////////////////////////////////////////////////

void SetupEditor::load()
{
    // reflect changes to referenced object names
    refreshAllowedValues();
    
    // build a list of names for the object selector
    juce::Array<juce::String> names;
    // clone the Setup list into a local copy
    setups.clear();
    revertSetups.clear();
    MobiusConfig* config = supervisor->getMobiusConfig();
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
    context->setObjectNames(names);

    // load the first one, do we need to bootstrap one if
    // we had an empty config?
    selectedSetup = 0;
    selectedTrack = 0;
    loadSetup(selectedSetup);

    // if we've had the panel open before, it will keep the radio
    // from the last time, have to put it back to match selectedTrack
    adjustTrackSelector();
}

/**
 * Each time the form is loaded for a new session, we need to refresh
 * the previously initialized Fields that have object names in them.
 * For Setup this is the group name and track preset.
 */
void SetupEditor::refreshAllowedValues()
{
    if (groupField != nullptr)
      groupField->refreshAllowedValues();

    if (trackPresetField != nullptr)
      trackPresetField->refreshAllowedValues();
    
    if (defaultPresetField != nullptr)
      defaultPresetField->refreshAllowedValues();
}

/**
 * Refresh the object selector on initial load and after any
 * objects are added or removed.
 */
void SetupEditor::refreshObjectSelector()
{
    juce::Array<juce::String> names;
    for (auto setup : setups) {
        if (setup->getName() == nullptr)
          setup->setName("[New]");
        names.add(setup->getName());
    }
    context->setObjectNames(names);
    context->setSelectedObject(selectedSetup);
}

/**
 * Adjust the track selector Radio prior to loading to reflect
 * changes to the configured track count.  During static initialization
 * this was built out for 8 tracks.
 * ugh, this doesn't work, the radio just keeps going into the void and doesn't
 * size down, and the labels don't gain a 10th digit
 * probably an issue with the old SimpleRadio, but rather than trying to fix that
 * just switch over to a combo box if the size is more than 8.
 *
 * This transition only happens once, then it's combo all the way.
 * This is really ugly, but it gets the one user that needs more than 8 tracks
 * something usable.
 */
void SetupEditor::adjustTrackSelector()
{
    MobiusConfig* config = supervisor->getMobiusConfig();

    // setups only apply to core tracks so it is permissible to use this
    // rather than view->totalTracks
    int ntracks = config->getCoreTracks();
    if (ntracks > 8 && ntracks <= 32 && trackCombo == nullptr) {

        // it has been a radio
        // gak, Form/FormPanel is a mess, it requires a dynamic object that it
        // takes ownership of
        // SimpleRadio had it's own label, but since we're not in control of
        // layout, it's easier just to put the "label" in the item names
        trackCombo =  new juce::ComboBox();
        for (int i = 1 ; i <= ntracks ; i++) {
            trackCombo->addItem("Track " + juce::String(i), i);
        }
        trackCombo->addListener(this);

        // shit, Form/Field was built in the days where components sized themselves
        // raw ComboBox doesn't do that
        trackCombo->setSize(100, 20);

        // this will also delete the former trackSelector radio
        FormPanel* formPanel = form.getPanel("Tracks");
        formPanel->replaceHeader(trackCombo);
        trackSelector = nullptr;
    }

    if (trackSelector != nullptr)
      trackSelector->setSelection(selectedTrack);
    else if (trackCombo != nullptr)
      trackCombo->setSelectedId(selectedTrack + 1);
}

/**
 * Called by the Save button in the footer.
 * 
 * Save all setups that have been edited during this session
 * back to the master configuration.
 *
 * Tell the ConfigEditor we are done.
 */
void SetupEditor::save()
{
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
    revertSetups.clear();
    
    MobiusConfig* config = supervisor->getMobiusConfig();
    // this will also delete the current setup list
    config->setSetups(plist);

    // this flag is necessary to get the engine to pay attention
    config->setupsEdited = true;
    
    supervisor->updateMobiusConfig();
}

/**
 * Throw away all editing state.
 */
void SetupEditor::cancel()
{
    // delete the copied setups
    setups.clear();
    revertSetups.clear();
}

void SetupEditor::revert()
{
    Setup* revert = revertSetups[selectedSetup];
    if (revert != nullptr) {
        Setup* reverted = new Setup(revert);
        setups.set(selectedSetup, reverted);
        // what about the name?
        loadSetup(selectedSetup);
        refreshObjectSelector();
    }
}

//////////////////////////////////////////////////////////////////////
//
// ObjectSelector overloads
// 
//////////////////////////////////////////////////////////////////////

/**
 * Called when the combobox changes.
 */
void SetupEditor::objectSelectorSelect(int ordinal)
{
    if (ordinal != selectedSetup) {
        saveSetup(selectedSetup);
        selectedSetup = ordinal;
        loadSetup(selectedSetup);
    }
}

void SetupEditor::objectSelectorNew(juce::String newName)
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
    
    selectedSetup = newOrdinal;
    loadSetup(selectedSetup);
    refreshObjectSelector();
}

/**
 * Delete is somewhat complicated.
 * You can't undo it unless we save it somewhere.
 * An alert would be nice, ConfigPanel could do that.
 */
void SetupEditor::objectSelectorDelete()
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
        refreshObjectSelector();
    }
}

void SetupEditor::objectSelectorRename(juce::String newName)
{
    Setup* setup = setups[selectedSetup];
    setup->setName(newName.toUTF8());
}

//////////////////////////////////////////////////////////////////////
//
// Internal Methods
// 
//////////////////////////////////////////////////////////////////////

/**
 * Load a setup into the parameter fields
 */
void SetupEditor::loadSetup(int index)
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
void SetupEditor::saveSetup(int index)
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

Setup* SetupEditor::getSelectedSetup()
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

void SetupEditor::render()
{
    initForm();
    form.render();

    // the number of tracks is configurable but since render() is called
    // by the static constructor we won't have the MobiusConfig available yet
    // start with the default of 8 tracks and deal with it later during load()
    trackSelector = new SimpleRadio();
    trackCount = 8;
    juce::StringArray trackNumbers;
    for (int i = 0 ; i < trackCount ; i++) {
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
    addAndMakeVisible(form);
}

void SetupEditor::initForm()
{
    // missing:
    // resetRetains, overlayBindings

    addField("Tracks", UIParameterTrackName);
    addField("Tracks", UIParameterSyncSource);    // overrides default SyncSourceParameter
    addField("Tracks", UIParameterTrackSyncUnit);
    // same hackery as UIParameterGroupName to get it refreshed every time
    trackPresetField = new ParameterField(supervisor, UIParameterTrackPreset);
    form.add(trackPresetField, "Tracks", 0);

    // severe hackery to make group names look like Structures
    //addField("Tracks", UIParameterGroup);
    // this also needs to be refreshed on every load to track group renames
    groupField = new ParameterField(supervisor, UIParameterGroupName);
    form.add(groupField, "Tracks", 0);
    
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
    addField("Synchronization", UIParameterMuteSyncMode);
    addField("Synchronization", UIParameterResizeSyncAdjust);
    addField("Synchronization", UIParameterSpeedSyncAdjust);
    addField("Synchronization", UIParameterMinTempo);
    addField("Synchronization", UIParameterMaxTempo);
    addField("Synchronization", UIParameterManualStart);

    // Other
    // this needs to be done in a more obvious way

    addField("Other", UIParameterActiveTrack);
    defaultPresetField = new ParameterField(supervisor, UIParameterDefaultPreset);
    form.add(defaultPresetField, "Other", 0);

    // Binding Overlay
}

void SetupEditor::addField(const char* tab, UIParameter* p)
{
    form.add(new ParameterField(supervisor, p), tab, 0);
}

//////////////////////////////////////////////////////////////////////
//
// Listneners
//
//////////////////////////////////////////////////////////////////////

/**
 * Respond to the track selection radio
 */
void SetupEditor::radioSelected(SimpleRadio* r, int index)
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
void SetupEditor::buttonClicked(juce::Button* b)
{
    Trace(1, "Button %s\n", b->getButtonText().toUTF8());
}

void SetupEditor::comboBoxChanged(juce::ComboBox* c)
{
    int id = c->getSelectedId();
    int trackNumber = id - 1;

    saveSetup(selectedSetup);
    selectedTrack = trackNumber;
    loadSetup(selectedSetup);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
