/**
 * A container of StripElements displaying state for one track.
 * These can be used in two contexts, in the "docked" track
 * strip with parent TrackStrips, and in the "floating"
 * track strip with the parent FloatingStripElement.
 *
 * "Dockness" then becomes a side effect of being within TrackStrips
 * and "floating" is being within a StatusAreaWrapper.
 */

#include <JuceHeader.h>

#include "../../Provider.h"
#include "../../AudioClerk.h"

#include "../../util/Trace.h"
#include "../../model/UIConfig.h"
#include "../../model/Symbol.h"
#include "../MobiusView.h"

#include "Colors.h"
#include "StripElement.h"
#include "StripElements.h"
#include "TrackStrips.h"
#include "TrackStrip.h"
#include "FloatingStripElement.h"
#include "UIElementStripAdapter.h"
#include "UIElement.h"
#include "UIElementFactory.h"

// eventually have one that takes a StatusAreaWrapper parent
TrackStrip::TrackStrip(TrackStrips* parent)
{
    setName("TrackStrip");
    strips = parent;
    floater = nullptr;

    // prepare the track selection action
    // todo: need to refine the difference between activating a track
    // with and without "empty track actions"
    // maybe TrackSelect vs. TrackSwitch
    trackSelectAction.symbol = strips->getProvider()->getSymbols()->intern("SelectTrack");
}

TrackStrip::TrackStrip(FloatingStripElement* parent)
{
    setName("TrackStrip");
    floater = parent;
    strips = nullptr;
}

TrackStrip::~TrackStrip()
{
}

MobiusView* TrackStrip::getMobiusView()
{
    MobiusView* view = nullptr;
    
    if (strips != nullptr)
      view = strips->getMobiusView();
    else if (floater != nullptr)
      view = floater->getMobiusView();
    else {
        // can't happen, bad day
    }
    return view;
}

MobiusViewTrack* TrackStrip::getTrackView()
{
    MobiusView* view = getMobiusView();
    return view->getTrack(followTrack);
}
          
Provider* TrackStrip::getProvider()
{
    if (strips != nullptr)
      return strips->getProvider();

    if (floater != nullptr)
      return floater->getProvider();

    // someone is about to have a bad day
    return nullptr;
}

/**
 * Set the track to follow, -1 means the active track.
 * For floaters, could give them a component to select the track.
 * Note that unlike Mobius core and binding scopes, track numbers
 * are zero based here, so they can be used as indexes into track array
 * in MobiusView.
 */
void TrackStrip::setFollowTrack(int t)
{
    followTrack = t;
    // won't set this after construction so don't need to repaint
}

/**
 * If we're a floating strip, this is the number of the floating
 * strip configuration to pull out of UIConfig.
 * Currently there are only two but we'll allow more.
 * The number is zero based.
 */
void TrackStrip::setFloatingConfig(int i)
{
    floatingConfig = i;
}

/**
 * If we follow a speicfic track return it.
 * If we're floating must have remembered it.
 */
int TrackStrip::getTrackIndex()
{
    int tnum = 0;
    if (followTrack >= 0) {
        tnum = followTrack;
    }
    else {
        // update needs to have saved it
        tnum = focusedTrack;
    }
    return tnum;
}

/**
 * Preferred width is the max of all the child widths.
 */
int TrackStrip::getPreferredWidth()
{
    int maxWidth = 0;

    for (auto el : elements) {
        // position -1 means not assigned,
        // actually should be the same as !visible
        if (el->position >= 0) {
            int width = el->getPreferredWidth();
            if (width > maxWidth)
              maxWidth = width;
        }
    }
    // border
    maxWidth += 4;
    
    return maxWidth;
}
        
int TrackStrip::getPreferredHeight()
{
    int maxHeight = 0;

    for (auto el : elements) {
        if (el->position >= 0) {
            maxHeight += el->getPreferredHeight();
        }
    }

    // border
    maxHeight += 4;
    
    return maxHeight;
}

/**
 * Todo: have notes somewhere about capturing the initial size
 * percentages and trying to retain that.  Here we'll keep the original sizes
 * but center them within the strip.
 */
void TrackStrip::resized()
{
    // offset for border
    int leftOffset = 2;
    int topOffset = 2;
    int maxWidth = getWidth() - 4;

    juce::Array<StripElement*> sorted;
    // this is the maximum number we can have, but we may use less
    // if some were disabled
    sorted.resize(elements.size());
    for (auto el : elements) {
        if (el->position >= 0)
          sorted.set(el->position, el);
    }

    // now iterate over the sorted list, if we hit nullptr we're done
    for (auto el : sorted) {
        if (el == nullptr)
          break;
        else {
            int width = el->getPreferredWidth();
            int height = el->getPreferredHeight();
            int indent = (maxWidth - width) / 2;
            
            el->setBounds(leftOffset + indent, topOffset, width, height);
            
            topOffset += height;
        }
    }
}

void TrackStrip::update(MobiusView* view)
{
    // sub elements track changes themselves
    for (int i = 0 ; i < elements.size() ; i++) {
        StripElement* el = elements[i];
        el->update(view);
    }

    // outer strip container may need to repaint
    // if it needs border changes
    // this will also request repaints of all the children
    // which may have already requested a repaint
    // todo: verify that this doesn't cause children
    // to be repainted twice
    MobiusViewTrack* track = getTrackView();
    
    if (focusedTrack != view->focusedTrack ||
        lastActive != track->active ||
        lastDropTarget != outerDropTarget) {
        
        focusedTrack = view->focusedTrack;
        lastActive = track->active;
        lastDropTarget = outerDropTarget;
        repaint();
    }
}

void TrackStrip::paint(juce::Graphics& g)
{
    if (strips != nullptr) {
        // we're in the dock, border shows active
        if (outerDropTarget) {
            g.setColour(juce::Colours::red);
            g.drawRect(getLocalBounds(), 2);
        }
        else {
            MobiusViewTrack* tview = getTrackView();
            if (focusedTrack == followTrack) {
                g.setColour(juce::Colours::white);
                g.drawRect(getLocalBounds(), 2);
            }
            else if (tview->active) {
                g.setColour(juce::Colours::grey);
                g.drawRect(getLocalBounds(), 2);
            }
        }
    }
    else {
        // floater paints itself
        //g.setColour(juce::Colours::white);
        //g.drawRect(getLocalBounds(), 1);
    }
}

/**
 * Allow clicking in the docked strip to activate the track.
 * This will only be called directly by Juce if you click outside
 * the bounds of one of the child StripElements.  StripElement also
 * overrides mouseDown and forwards up here.
 *
 * The elements with sub-components like StripRotary won't support this
 * since Juce goes bottom-up handling mouse events.  But at least most of them
 * will work.
 *
 * todo: will want subclass specific click beyavior.
 * TrackNumber used to also control focus, and LoopStack could switch loops.
 */
void TrackStrip::mouseDown(const juce::MouseEvent& event)
{
    (void)event;
    if (isDocked()) {
        // action argument is the track number, 1 based
        // for some reason, the UI uses 0 based numbers with -1 meaning active
        // I guess to make it easier to use the numbers as indexes into
        // the MobiusView/MobiusState track array
        trackSelectAction.value = getTrackIndex() + 1;
        getProvider()->doAction(&trackSelectAction);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Configuration
//
//////////////////////////////////////////////////////////////////////

/**
 * Pull out the appropriate configuration, docked or floating.
 * 
 * Reconile the DisplayStrip definition with the current set of
 * child components.  Any existing components not enabled in the DispayStrip
 * are marked invisible (but not deleted).  Any DisplayStrip elements
 * that don't have components get a new one added to the child list.
 *
 * Keeping the existing components and simply hiding or showing them results
 * in less display flicker, and allows us to retain their original position
 * when hiddeen so it can be restored if put back.
 *
 * Order of elements in the strip is important.  Rather than moving objects
 * within the parent's component list, we save the vertical location of each
 * element in the StripEleemnt, then resized() uses that to position them
 * in the correct order.  This saves having resized() walk over the definition
 * again.
 */
void TrackStrip::configure()
{
    UIConfig* config = getProvider()->getUIConfig();
    DisplayLayout* layout = config->getActiveLayout();
    DisplayStrip* strip = nullptr;
    
    if (strips != nullptr) {
        // we're docked
        strip = layout->getDockedStrip();
    }
    else {
        // we're floating
        // formerly had two of these, not in the new model
        // rather than fixing this as 1 and 2, support any number of them
        strip = layout->getFloatingStrip();
    }

    if (strip != nullptr) {
        // vertical strip order
        int position = 0;
        
        for (auto displayElement : strip->elements) {
            // find an existing StripElement component with a static definition
            // that matches the name in the configuration
            // todo: eventually use DisplayElementDefinition consistently for this
            // the name of the DisplayElement must be the same as the name
            // in a static StripElementDefinition object that was stored on the Component
            const char* definitionName = displayElement->name.toUTF8();
            StripElementDefinition* def = StripElementDefinition::find(definitionName);

            // this is what is used for the new UIElement-based StripElements, and what
            // all of them should eventually use
            UIElementDefinition* newdef = nullptr;
            if (def == nullptr)
              newdef = config->findDefinition(displayElement->name);
                

            if (def == nullptr && newdef == nullptr) {
                Trace(1, "TrackStrip: Unknwon StripElementDefinition name %s\n", definitionName);
            }
            else {
                // locate a child with the same definition
                StripElement* child = nullptr;
                for (auto el : elements) {
                    // assuming can have only one with the same definition
                    // if we ever support more than one instance per definition, which
                    // would happen once we allow multiple floating strips or other
                    // container, will need a way to identify them

                    if (def != nullptr && el->getDefinition() == def) {
                        child = el;
                        break;
                    }
                    else if (newdef != nullptr && el->getName() == newdef->name) {
                        child = el;
                        break;
                    }
                }

                if (child == nullptr) {
                    // haven't seen this one yet
                    // if the DisplayElement is disabled, don't make one just to hide it
                    if (!displayElement->disabled) {
                        if (def != nullptr)
                          child = createStripElement(def);
                        else if (newdef != nullptr)
                          child = createNewStripElement(newdef);
                        
                        if (child != nullptr) {
                            addAndMakeVisible(child);
                            elements.add(child);
                        }
                    }
                }
                else {
                    // already had this, adjust visibility
                    child->setVisible(!displayElement->disabled);
                }

                if (child != nullptr) {
                    if (child->isVisible()) {
                        // store the next vertical position
                        child->position = position;
                        position++;
                    }
                    else {
                        // went or remained invisible, remove
                        // position so we don't confuse resized()
                        child->position = -1;
                    }
                }
            }
        }

        // now let any visible children reconfigure
        // the only one that cares is LoopStack
        for (auto el : elements) {
            if (el->isVisible()) {
                el->configure();
            }
        }
    }
    else {
        // no matching strip, shouldn't happen, if we have any children
        // which REALLY shouldn't happen hide them
        if (elements.size() > 0)
          Trace(1, "TrackStrip: Strip has child components and no DisplayStrip definition\n");
    }

    // force a resized to get any order changes
    resized();
}

/**
 * Build the right StripElement instance for a StripElementDefinition.
 * This is obviously really horible and cries out for a lambda.
 * A cry that goes unanswered.
 */
StripElement* TrackStrip::createStripElement(StripElementDefinition* def)
{
    StripElement* el = nullptr;
    
    if (def == StripDefinitionTrackNumber) {
        el = new StripTrackNumber(this);
    }
    else if (def == StripDefinitionMaster) {
        el = new StripMaster(this);
    }
    else if (def == StripDefinitionFocusLock) {
        el = new StripFocusLock(this);
    }
    else if (def == StripDefinitionLoopRadar) {
        el = new StripLoopRadar(this);
    }
    else if (def == StripDefinitionLoopThermometer) {
        el = new StripLoopThermometer(this);
    }
    else if (def == StripDefinitionOutput) {
        el = new StripOutput(this);
    }
    else if (def == StripDefinitionInput) {
        el = new StripInput(this);
    }
    else if (def == StripDefinitionFeedback) {
        el = new StripFeedback(this);
    }
    else if (def == StripDefinitionAltFeedback) {
        el = new StripAltFeedback(this);
    }
    else if (def == StripDefinitionPan) {
        el = new StripPan(this);
    }
    else if (def == StripDefinitionLoopStack) {
        el = new StripLoopStack(this);
    }
    else if (def == StripDefinitionOutputMeter) {
        el = new StripOutputMeter(this);
    }
    else if (def == StripDefinitionInputMeter) {
        el = new StripInputMeter(this);
    }
    else if (def == StripDefinitionGroupName) {
        el = new StripGroupName(this);
    }
    else {
        Trace(1, "TrackStrip: Unsupported StripElementDefinition %s\n", def->getName());
    }
    return el;
}

StripElement* TrackStrip::createNewStripElement(UIElementDefinition* def)
{
    StripElement* element = nullptr;

    UIElement* uie = UIElementFactory::create(getProvider(), def);
    if (uie != nullptr) {
        // unlike use as a StatusElement, these will have a specific scope
        // the number here is what is used to scope a Query
        if (followTrack >= 0)
          uie->setScope(followTrack+1);
        
        // temporary: wrap it in something that makes it look
        // like a StatusElement
        element = new UIElementStripAdapter(this, uie);
        // once this is added as a child, it stays there and is enabled or
        // disabled, this name is how configure() finds it
        // note: TrackStrip uses a different convention for finding children than StatusArea
        // SA uses the ComponentID and TS uses name, set both
        element->setComponentID(def->name);
        element->setName(def->name);

        // configure() will call addChildComponent and elements.add
    }
    
    return element;
}
        
/**
 * Called by one of the sub elements to perform an action.
 * Here we'll add the track scope and pass it along up
 */
void TrackStrip::doAction(UIAction* action)
{
    int scope;
    if (followTrack == -1) {
        MobiusViewTrack* tv = getTrackView();
        scope = tv->index + 1;
    }
    else {
        scope = followTrack + 1;
    }
    
    action->setScopeTrack(scope);
    
    getProvider()->doAction(action);
}

//
// Drag and Drop Files
//

bool TrackStrip::isInterestedInFileDrag(const juce::StringArray& files)
{
    (void)files;
    // only if we're in the dock
    return (strips != nullptr);
}

void TrackStrip::fileDragEnter(const juce::StringArray& files, int x, int y)
{
    (void)files;
    (void)x;
    (void)y;
    //Trace(2, "TrackStrip: fileDragEnter\n");
    outerDropTarget = true;
}

void TrackStrip::fileDragMove(const juce::StringArray& files, int x, int y)
{
    (void)files;
    (void)x;
    (void)y;
}

void TrackStrip::fileDragExit(const juce::StringArray& files)
{
    (void)files;
    //Trace(2, "TrackStrip: fileDragExit\n");
    outerDropTarget = false;
}

void TrackStrip::filesDropped(const juce::StringArray& files, int x, int y)
{
    (void)x;
    (void)y;
    Trace(2, "TrackStrip: filesDropped into track %d\n", followTrack);
    outerDropTarget = false;
    
    AudioClerk* clerk = getProvider()->getAudioClerk();
    // track/loop numbers are 1 based, with zero meaning "active"
    // followTrack is zero based
    clerk->filesDropped(files, followTrack + 1, 0);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
