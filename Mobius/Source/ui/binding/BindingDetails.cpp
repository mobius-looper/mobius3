/**
 * Oddities on the argument combos:
 *
 * TrackGroup
 *
 * TrackGroup can be sent to the Focused track or any track by number so
 * it needs a scope selector.  The scope selector ALSO includes group names
 * which is functional but looks weird.  You have a group binding to A that changes the
 * tracks in that group to a different group.
 *
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/MidiUtil.h"

#include "../../model/Binding.h"
#include "../../model/GroupDefinition.h"
#include "../../model/Scope.h"
#include "../../model/Symbol.h"
#include "../../model/FunctionProperties.h"

#include "../../Supervisor.h"
#include "../../KeyTracker.h"
#include "../../MidiManager.h"

#include "../JuceUtil.h"

#include "BindingUtil.h"

#include "BindingDetails.h"

#define SectionHeight 20

BindingDetailsPanel::BindingDetailsPanel()
{
    // don't really need a title on these
    // but without a title bar you don't get mouse
    // events for dragging, unless you use followContentMouse
    //setTitle("Binding");

    setContent(&content);

    // this gives it dragability within the entire window since
    // these don't have a title bar
    followContentMouse();

    // this gives it a yellow border
    // too distracting
    //setAlert();

    resetButtons();
    addButton(&saveButton);
    addButton(&cancelButton);

    setBackground(juce::Colours::black);
    setBorderColor(juce::Colours::lightgrey);
    
    setSize(350,400);
}

void BindingDetailsPanel::show(juce::Component* parent, BindingDetailsListener* l, Binding* b)
{
    // since Juce can't seem to control z-order, even if we already have this parent
    // (which is unlikely), remove and add it so it's at the top
    juce::Component* current = getParentComponent();
    if (current != nullptr)
      current->removeChildComponent(this);
    parent->addAndMakeVisible(this);

    content.load(l, b);
    
    // why was this necessary?
    content.resized();
    JuceUtil::centerInParent(this);
    BasePanel::show();
}

void BindingDetailsPanel::close()
{
    juce::Component* current = getParentComponent();
    if (current != nullptr) {
        current->removeChildComponent(this);
    }
}

void BindingDetailsPanel::initialize(Supervisor* s)
{
    content.initialize(s);
}

void BindingDetailsPanel::footerButton(juce::Button* b)
{
    if (b == &saveButton) {
        content.save();
    }
    else if (b == &cancelButton) {
        content.cancel();
    }
    
    if (b == &saveButton || b == &cancelButton)
      close();
}

//////////////////////////////////////////////////////////////////////
//
// Content
//
//////////////////////////////////////////////////////////////////////

BindingContent::BindingContent()
{
    title.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(title);

    triggerTitle.setText("Trigger", juce::NotificationType::dontSendNotification);
    triggerTitle.setColour(juce::Label::textColourId, juce::Colours::yellow);
    triggerTitle.setFont(JuceUtil::getFont(SectionHeight));
    addAndMakeVisible(triggerTitle);
    
    targetTitle.setText("Options", juce::NotificationType::dontSendNotification);
    targetTitle.setColour(juce::Label::textColourId, juce::Colours::yellow);
    targetTitle.setFont(JuceUtil::getFont(SectionHeight));
    
    addAndMakeVisible(targetTitle);

    juce::StringArray midiTypeNames;
    // could have an array of Triggers for these
    midiTypeNames.add("Note");
    midiTypeNames.add("Control");
    midiTypeNames.add("Program");
    midiType.setItems(midiTypeNames);

    // Binding number is the combo index where
    // zero means "any"
    juce::StringArray channelNames;
    channelNames.add("Any");
    for (int i = 1 ; i <= 16 ; i++) {
        channelNames.add(juce::String(i));
    }
    midiChannel.setItems(channelNames);

    // form fields are added during load()
    addChildComponent(triggerForm);

    addAndMakeVisible(qualifiers);
}

BindingContent::~BindingContent()
{
    closeTrackers();
}

void BindingContent::trackKeys()
{
    if (supervisor != nullptr) {
        // use the newer "exclusive" listener to prevent Binderator
        // from going crazy while we capture key events
        //KeyTracker::addListener(this);
        supervisor->getKeyTracker()->setExclusiveListener(this);
    }
}

void BindingContent::trackMidi()
{
    if (supervisor != nullptr) {
        MidiManager* mm = supervisor->getMidiManager();
        mm->addMonitor(this);
    }
}

void BindingContent::closeTrackers()
{
    if (supervisor != nullptr) {

        supervisor->getKeyTracker()->removeExclusiveListener(this);

        MidiManager* mm = supervisor->getMidiManager();
        mm->removeMonitor(this);
    }
}

void BindingContent::initialize(Supervisor* s)
{
    supervisor = s;
    //tree.initialize(p);
}

void BindingContent::cancel()
{
    closeTrackers();
    if (listener != nullptr)
      listener->bindingCanceled();
}

void BindingContent::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    area.removeFromTop(8);

    int titleHeight = 20;
    title.setFont(JuceUtil::getFont(titleHeight));
    title.setBounds(area.removeFromTop(titleHeight));

    area.removeFromTop(20);
    area.removeFromLeft(15);

    if (type == TypeHost) {
        // no interesting trigger fields
    }
    else if (type == TypeButton) {
        // todo: display name, but is that really a Trigger?
    }
    else if (type != TypeUnknown) {

        triggerTitle.setBounds(area.removeFromTop(SectionHeight));
        area.removeFromTop(20);

        juce::Rectangle<int> triggerArea = area.removeFromTop(triggerForm.getPreferredHeight());
        triggerArea.removeFromLeft(20);
        triggerForm.setBounds(triggerArea);
        area.removeFromTop(20);
    }

    targetTitle.setBounds(area.removeFromTop(SectionHeight));
    
    area.removeFromTop(20);

    juce::Rectangle<int> qualifierArea = area.removeFromTop(qualifiers.getPreferredHeight());
    qualifierArea.removeFromLeft(20);
    qualifiers.setBounds(qualifierArea);
}

//////////////////////////////////////////////////////////////////////
//
// Load
//
//////////////////////////////////////////////////////////////////////

void BindingContent::load(BindingDetailsListener* l, Binding* b)
{
    listener = l;
    binding = b;

    // capture these for use in building the fields
    maxTracks = supervisor->getSession()->getTrackCount();

    SymbolTable* symbols = supervisor->getSymbols();
    juce::String prefix;
    Symbol* s = symbols->find(b->symbol);
    if (s == nullptr)
      prefix = "???: ";
    else if (s->functionProperties != nullptr)
      prefix = "Function: ";
    else if (s->parameterProperties != nullptr)
      prefix = "Parameter: ";
    else if (s->script != nullptr)
      prefix = "Script: ";
    else if (s->sample != nullptr)
      prefix = "Sample: ";
    else if (s->behavior ==  BehaviorActivation)
      prefix = "";
    else
      prefix = "???: ";
    
    title.setText(prefix + b->symbol, juce::NotificationType::dontSendNotification);

    // since the form is a member object and we reubild it every time,
    // it must be cleared first
    triggerForm.clear();

    if (b->trigger == Binding::TriggerUnknown) {
        Trace(1, "BindingContent: Trigger not set on binding");
    }
    else if (b->trigger == Binding::TriggerKey) {
        type = TypeKey;
    }
    else if (b->trigger == Binding::TriggerNote ||
             b->trigger == Binding::TriggerControl ||
             b->trigger == Binding::TriggerProgram) {
        type = TypeMidi;

        triggerForm.add(&midiType);
        triggerForm.add(&midiChannel);
    }
    else if (b->trigger == Binding::TriggerHost) {
        // nothing specific atm, maybe the unique parameter id?
        type = TypeHost;
    }
    else if (b->trigger == Binding::TriggerUI) {
        type = TypeButton;
    }
    else {
        Trace(1, "BindingContent: Unsupported trigger type %d", (int)(b->trigger));
    }

    if (type == TypeKey || type == TypeMidi) {
        triggerForm.add(&triggerValue);
        triggerForm.add(&release);
        triggerForm.add(&capture);
        captureText.setAdjacent(true);
        captureText.setNoBorder(true);
        
        // this is the same color as used by BasePanel to make the capture
        // text stand out less than a black input field
        // !! need to be sharing this color
        // captureText.setBackgroundColor(juce::Colours::darkgrey.darker(0.8f));
        captureText.setBackgroundColor(juce::Colours::black);
        
        triggerForm.add(&captureText);
        
        triggerForm.setVisible(true);

        release.setValue(b->release);
        arguments.setValue(b->arguments);
        
        capture.setValue(capturing);
        captureText.setValue("");
    }

    if (type == TypeKey) {
        triggerValue.setValue(BindingUtil::renderTrigger(b));
    }
    else if (type == TypeMidi) {
        Binding::Trigger trigger = b->trigger;
        if (trigger == Binding::TriggerNote) {
            midiType.setSelection(0);
        }
        else if (trigger == Binding::TriggerControl) {
            midiType.setSelection(1);
        }
        else if (trigger == Binding::TriggerProgram) {
            midiType.setSelection(2);
        }
        else {
            // shouldn't be here, go back to Note
            Trace(1, "BindingContent: Invalide trigger type in MIDI form");
            midiType.setSelection(0);
        }
        
        midiChannel.setSelection(b->midiChannel);

        // todo: Capture is going to display symbolic names
        // but the value is a raw number, need one or the other
        // or both
        triggerValue.setValue(juce::String(b->triggerValue));
    }

    // YanForm needs to restructure itself if the contents change but the outer
    // area doesn't, resized() alone won't do that
    triggerForm.forceResize();

    qualifiers.clear();

    bool addScope = false;
    if (s->parameterProperties != nullptr) {
        // should only be including track-related parameters
        // in the tree so don't have much filtering to do here
        addScope = true;
    }
    else if (s->functionProperties != nullptr) {
        addScope = (!s->functionProperties->global &&
                    !s->level == LevelUI);
    }

    if (addScope) {
        qualifiers.add(&scope);
        refreshScopeNames();
        refreshScopeValue(b);
    }

    if (s->functionProperties != nullptr) {
        FunctionProperties* props = s->functionProperties.get();
        if (props->hasArguments ||
            props->argumentLabel.length() > 0 ||
            props->argumentValue.length() > 0) {

            YanField* field = renderArguments(b, props);
            qualifiers.add(field);
            
            juce::String label = props->argumentLabel;
            if (label.length() == 0)
              label = "Arguments";
            field->setLabel(label);
        }
    }

    qualifiers.forceResize();

    resized();

    if (type == BindingContent::TypeKey)
      trackKeys();
    else if (type == BindingContent::TypeMidi)
      trackMidi();
}

/**
 * Sure would be nice to have a YanField that could handle
 * this kind fo render switching.
 */
YanField* BindingContent::renderArguments(Binding* b, FunctionProperties* props)
{
    YanField* result = nullptr;

    argumentType = props->argumentValue;
    argumentNone = false;
    
    if (argumentType == "loopNumber") {
        renderLoopNumber(b);
        result = &argumentCombo;
    }
    else if (argumentType == "trackNumber") {

        juce::String none = props->argumentNone;
        if (none.length() > 0)
          argumentNone = true;
        
        renderTrackNumber(b, none);
        result = &argumentCombo;
    }
    else if (argumentType == "trackGroup") {
        renderTrackGroup(b);
        result = &argumentCombo;
    }
    else {
        argumentType = "";
        result = &arguments;
    }

    return result;
}

void BindingContent::addTrackNumbers(juce::String prefix, juce::StringArray& items)
{
    // saved maxtracks at the start of load()
    for (int i = 1 ; i <= maxTracks ; i++)
      items.add(prefix + juce::String(i));
}

void BindingContent::addGroupNames(juce::String prefix, juce::StringArray& items)
{
    GroupDefinitions* container = supervisor->getGroupDefinitions();
    for (auto g : container->groups)
      items.add(prefix + g->name);
}

void BindingContent::renderLoopNumber(Binding* b)
{
    // hmm, the maxLoops parameter got lost along the way
    // this should be more than enough for most people
    int maxLoops = 8;
    
    juce::StringArray items;
    for (int i = 1 ; i <= maxLoops ; i++)
      items.add(juce::String(i));

    int selection = 0;
    if (b->arguments.length() > 0) {
        selection = items.indexOf(b->arguments);
        if (selection < 0) {
            // out of range loop numbers are far less common than track numbers
            // or group names, and is considered an error, fall back to loop 1
            Trace(1, "BindingDetails: Loop number in binding out of range %d",
                  b->arguments.getIntValue());
            selection = 0;
        }
    }

    argumentCombo.setItems(items);
    argumentCombo.setSelection(selection);
}

void BindingContent::renderTrackNumber(Binding* b, juce::String none)
{
    juce::StringArray items;
    if (none.length() > 0) {
        items.add(none);
    }
    addTrackNumbers("", items);

    int selection = 0;
    if (b->arguments.length() > 0) {
        selection = items.indexOf(b->arguments);
        if (selection < 0) {
            // the binding had a track number that is out of range,
            // this can be normal if you swap between sessions that have
            // different numbers of tracks, and a binding created in the
            // larger session is edited in the smaller one
            // we could extend the item list to include it, but like
            // deleted track groups, you can't use it so stick it on the
            // end with a warning
            items.add(juce::String("Invalid: ") + b->arguments);
            selection = items.size() - 1;
        }
    }
    
    argumentCombo.setItems(items);
    argumentCombo.setSelection(selection);
}

void BindingContent::renderTrackGroup(Binding* b)
{
    juce::StringArray items;

    // these are special operator keywords
    items.add("clear");
    items.add("next");
    items.add("prev");

    addGroupNames("", items);
    
    // if the current name in the binding does not exist
    // in the list, it means the binding is old and the group it
    // referenced was deleted
    // we could add it to the list, but it won't do anything
    // they probably want to know though so stick a prefix on it
    int selection = 0;
    if (b->arguments.length() > 0) {
        selection = items.indexOf(b->arguments);
        if (selection < 0) {
            items.add(juce::String("Invalid: ") + b->arguments);
            selection = items.size() - 1;
        }
    }
    
    argumentCombo.setItems(items);
    argumentCombo.setSelection(selection);
}

/**
 * This needs to be done every time the form is displayed in order
 * to track group renames.  Not an issue right now since this
 * entire component is rebuilt every time.
 */
void BindingContent::refreshScopeNames()
{
    // scope always goes first
    juce::StringArray scopeNames;
    scopeNames.add("Focused");

    addTrackNumbers("Track ", scopeNames);
    addGroupNames("Group ", scopeNames);
    
    scope.setItems(scopeNames);
}

void BindingContent::refreshScopeValue(Binding* b)
{
    const char* scopeString = b->scope.toUTF8();
    int tracknum = Scope::parseTrackNumber(scopeString);
    if (tracknum > maxTracks) {
        // must be an old binding created before reducing
        // the track count, it reverts to global, should
        // have a more obvious warning in the UI
        Trace(1, "BindingContent: Binding scope track number out of range %d", tracknum);
    }
    else if (tracknum >= 0) {
        // element 0 is "global" so track number works
        scope.setSelection(tracknum);
    }
    else {
        GroupDefinitions* groups = supervisor->getGroupDefinitions();
        int index = groups->getGroupIndex(b->scope);
        if (index >= 0)
          scope.setSelection(maxTracks + 1 + index);
        else
          Trace(1, "BindingContent: Binding scope with unresolved group name %s", scopeString);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Save
//
//////////////////////////////////////////////////////////////////////

void BindingContent::save()
{
    save(binding);
    if (listener != nullptr)
      listener->bindingSaved();

    closeTrackers();
}

void BindingContent::save(Binding* b)
{
    if (type == TypeMidi) {

        switch (midiType.getSelection()) {
            case 0: b->trigger = Binding::TriggerNote; break;
            case 1: b->trigger = Binding::TriggerControl; break;
            case 2: b->trigger = Binding::TriggerProgram; break;
            default: break;
        }

        b->midiChannel = midiChannel.getSelection();
        b->triggerValue = triggerValue.getInt();
    }
    else if (type == TypeKey) {
        b->triggerValue = unpackKeyCode();
    }

    b->scope = unpackScope();
    b->arguments = unpackArguments();

    // only relevant for certain types
    if (b->trigger == Binding::TriggerKey || b->trigger == Binding::TriggerNote)
      b->release = release.getValue();
    else
      b->release = false;
}

/**
 * Undo the symbolic transformation to get back to
 * a raw key code
 */
int BindingContent::unpackKeyCode()
{
    juce::String value = triggerValue.getValue();
    int myCode = BindingUtil::unrenderKeyText(value);
    
    // capture has priority
    if (capture.getValue() && capturedCode > 0) {
        // test to see if there are any conditions where the text transform
        // doesn't end up  with the same thing
        if (capturedCode != myCode)
          Trace(1, "KeyboardEditor: Key encoding anomoly %d %d\n",
                capturedCode, myCode);
        myCode = capturedCode;
    }

    return myCode;
}

juce::String BindingContent::unpackScope()
{
    juce::String result;
    
    // item 0 is global, then tracks, then groups
    int item = scope.getSelection();
    if (item < 0) {
        // no selection, scope combo was not used
    }
    else if (item == 0) {
        // focused
    }
    else if (item <= maxTracks) {
        // track number
        result = juce::String(item);
    }
    else {
        // skip going back to the SystemConfig for the names and
        // just remove our prefix
        juce::String itemName = scope.getSelectionText();
        juce::String groupName = itemName.fromFirstOccurrenceOf("Group ", false, false);
        result = groupName;
    }

    return result;
}

juce::String BindingContent::unpackArguments()
{
    juce::String result;
    
    if (argumentType == "loopNumber") {
        // 1 based number just slides over
        result = argumentCombo.getSelectionText();
    }
    else if (argumentType == "trackNumber") {
        int trackNumber = argumentCombo.getSelection() + 1;
        // deal with the placeholder at the top to mean "no selection"
        if (argumentNone)
          trackNumber--;

        if (trackNumber > 0)
          result = juce::String(trackNumber);
    }
    else if (argumentType == "trackGroup") {
        // these all slide over literally, either operator
        // keywords or unqualfiied group names
        result = argumentCombo.getSelectionText();
    }
    else {
        // raw unadorned arguments
        result = arguments.getValue();
    }

    return result;
}

//////////////////////////////////////////////////////////////////////
//
// Trackers
//
//////////////////////////////////////////////////////////////////////

void BindingContent::setCapturing(bool b)
{
    capturing = b;
}

bool BindingContent::isCapturing()
{
    return capture.getValue();
}

void BindingContent::keyTrackerDown(int code, int modifiers)
{
    juce::String keytext = KeyTracker::getKeyText(code, modifiers);
    if (isCapturing()) {
        triggerValue.setValue(keytext);
        capturedCode = Binderator::getKeyQualifier(code, modifiers);
    }

    showCapture(keytext);
}

void BindingContent::keyTrackerUp(int code, int modifiers)
{
    (void)code;
    (void)modifiers;
}

void BindingContent::midiMonitor(const juce::MidiMessage& message, juce::String& source)
{
    (void)source;

    bool relevant = message.isNoteOn() || message.isController() || message.isProgramChange();

    if (relevant) {
        if (isCapturing()) {
            int value = -1;
            if (message.isNoteOn()) {
                midiType.setSelection(0);
                value = message.getNoteNumber();
            }
            else if (message.isController()) {
                midiType.setSelection(1);
                value = message.getControllerNumber();
            }
            else if (message.isProgramChange()) {
                midiType.setSelection(2);
                value = message.getProgramChangeNumber();
            }
#if 0        
            else if (message.isPitchWheel()) {
                messageType.setSelection(3);
                // value is a 14-bit number, and is not significant
                // since there is only one pitch wheel
                value = 0;

            }
#endif        
            if (value >= 0) {
                // channels are 1 based in Juce, 0 if sysex
                // Binding 0 means "any"
                // would be nice to have a checkbox to ignore the channel
                // if they want "any"
                int ch = message.getChannel();
                if (ch > 0)
                  midiChannel.setSelection(ch);
                triggerValue.setValue(juce::String(value));
            }
        }

        // whether we're capturing or not, tell BindingEditor about this
        // so it can display what is being captured when capture is off
        // sigh, need the equivalent of renderSubclassTrigger but we don't
        // have a binding
        juce::String cap = renderCapture(message);
        showCapture(cap);
    }
}

void BindingContent::showCapture(juce::String& s)
{
    captureText.setValue(s);
}

/**
 * Variant of renderTrigger used for capture.
 * Could share this with a little effort and ensure the formats
 * are consistent.
 */
juce::String BindingContent::renderCapture(const juce::MidiMessage& msg)
{
    juce::String text;

    // the menu displays channels as one based, not sure what
    // most people expect
    juce::String channel;
    if (msg.getChannel() > 0)
      channel = juce::String(msg.getChannel()) + ":";
    
    if (msg.isNoteOn()) {
        // old utility
        char buf[32];
        MidiNoteName(msg.getNoteNumber(), buf);
        text += channel;
        text += buf;
        // not interested in velocity
    }
    else if (msg.isProgramChange()) {
        text += channel;
        text += "Pgm ";
        text += juce::String(msg.getProgramChangeNumber());
    }
    else if (msg.isController()) {
        text += channel;
        text += "CC ";
        text += juce::String(msg.getControllerNumber());
    }
    return text;
}

bool BindingContent::midiMonitorExclusive()
{
    // return !isCapturePassthrough();
    return true;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
