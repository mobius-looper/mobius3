/**
 * Class managing most configuration editing dialogs.
 * Old Mobius implemented these with popup windows, we're now doing
 * these with simple Juce components overlayed over the main window.
 * 
 * See ConfigEditor.h for the interface with MainComponent.
 *
 * This wrapper allows for the possible experimentation with popup windows
 * if we ever decide to go there, isolating MainComponent from the details.
 *
 * There are some confusing dependencies on initialization since we're
 * trying to be a good boy and use RAII.  The sub panels are defined
 * as member objects which means they need to construct themselves
 * before the management hierarchy may be done stitching itself together.
 * In particular, panels shouldn't call up to Supervisor since that
 * may not be accessible at construction time.  The problem child
 * was ButtonPanel which wanted to load configuration which is really
 * too early for that anyway.  Defer until load()
 */

#include <JuceHeader.h>

#include "../../Supervisor.h"
#include "../../model/MobiusConfig.h"
#include "../../model/UIConfig.h"
#include "../JuceUtil.h"

#include "ConfigEditor.h"

ConfigEditor::ConfigEditor(juce::Component* argOwner)
{
    owner = argOwner;
}

/**
 * The main reason this exists is to connect the
 * editor to the Supervisor until I work out how to
 * do that in the constructor while letting it be passed
 * down the initialization chain.
 *
 * Also can defer adding the panels since we might want to
 * dynamically allocate those anyway.
 */
void ConfigEditor::init(Supervisor* super)
{
    supervisor = super;

    // add the various config panels to the owner but don't
    // make them visible yet
    addPanel(&audioDevices);
    addPanel(&display);
}

ConfigEditor::~ConfigEditor()
{
    // RIAA will destruct the various panels

    // this we own
    // todo: not any more, they are unused get rid of them
    //delete masterConfig;
    //delete masterUIConfig;
}

/**
 * Internal method to add the panel to the parent and put
 * it on the panel list for iteration.  The panel is
 * added but not made visible yet.
 */
void ConfigEditor::addPanel(ConfigPanel* p)
{
    owner->addChildComponent(p);
    panels.add(p);
}

void ConfigEditor::showAudioDevices()
{
    show(&audioDevices);
}

void ConfigEditor::showDisplay()
{
    show(&display);
}

void ConfigEditor::closeAll()
{
    // cancel any active editing state
    for (int i = 0 ; i < panels.size() ; i++) {
        ConfigPanel* p = panels[i];
        p->cancel();
    }

    // TODO: should this have the side effect of canceling the current editing session?
    show((ConfigPanel*)nullptr);
}

/**
 * Hide the currently active panel if any and show the desired one.
 * Subtle: because of the way MidiManager::setExclusiveListener/removeExclusiveListener
 * works it is important to hide all panels first, before showing the new one
 */
void ConfigEditor::show(ConfigPanel* selected)
{
    for (int i = 0 ; i < panels.size() ; i++) {
        ConfigPanel* other = panels[i];
        // note that this does not cancel an editing session, it
        // just hides it.  Some might want different behavior?
        if (other != selected) {
            if (other->isVisible()) {
                other->hiding();
                other->setVisible(false);
            }
        }
    }
    if (selected != nullptr) {
        if (!selected->isVisible()) {
            selected->showing();
            selected->setVisible(true);
        }
    }
    
    // weird for Juce but since we defer rendering and don't do it the normal way
    // resize just before showing
    selected->resized();
    selected->center();

    // ConfigPanel method to load the help catalog and other potentially
    // expensive things we avoid at construction time
    selected->prepare();
    
    // tell it to load state if it hasn't already
    selected->load();

    // JuceUtil::dumpComponent(selected);
}

//////////////////////////////////////////////////////////////////////
//
// ConfigPanel Callbacks
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by the panel when it is done.
 * There are three states a panel can be in:
 *
 *    unloaded: hasn't done anything
 *    loaded: has state loaded from the master config but hasn't changed anything
 *    changed: has unsaved changes
 *      
 * When a panel is closed by one of the buttons we look at the other
 * panels to see if they can be shown.  If any panel has unsaved
 * changes it will be shown.
 *
 * If no panel has unsaved changes but some of them have been loaded
 * we could either close everything, or show a loaded one.  The thinking
 * is that if someone bothered to show a panel, selected another without
 * changing anything, then closed the second panel, we can return to the
 * first one and let them continue.  Alternately, since they didn't bother
 * changing anything in the first one we could just close all of them.
 *
 * The first approach behaves more like a stack of panels which might
 * be nice.  The second is probably more obvious, if you open another without
 * doing anything in the first, you probably don't care about the first.
 * Let's start with the stack.
 *
 * Note though that this isn't actually a stack since we don't maintain
 * an ordered activation list if there are more than two loaded panels.
 */
void ConfigEditor::close(ConfigPanel* closing)
{
    ConfigPanel* nextLoaded = nullptr;
    ConfigPanel* nextChanged = nullptr;

    if (!closing->isVisible()) {
        // callback from something we asked to close
        // that wasn't visible
    }
    else {
        closing->hiding();
        closing->setVisible(false);
    
        for (int i = 0 ; i < panels.size() ; i++) {
            ConfigPanel* p = panels[i];
            if (p->isLoaded())
              nextLoaded = p;
            if (p->isChanged())
              nextChanged = p;
        }

        if (nextChanged != nullptr) {
            nextChanged->showing();
            nextChanged->setVisible(true);
        }
        else {
            // i don't think this needs to be configurable
            // but I want to try both behaviors easily
            bool showLoaded = true;

            if (showLoaded && nextLoaded != nullptr) {
                nextLoaded->showing();
                nextLoaded->setVisible(true);
            }
            else {
                // all done
            }
        }
    }
}

/**
 * Called by a panel read the MobiusConfig.
 * The master config object is managed by Supervisor.
 * The panels are allowed to make modifications to it
 * and ask us to save it.  Each panel must not overlap
 * on the changes it makes to the MobiusConfig.
 *
 * Might be better to have the panel return us just the
 * changes and have us splice it into the master config?
 *
 * todo: I'm not loving the control flow here, might be
 * better to always make a copy of the Supervisor's objects
 * for editing, but it isn't really necessary since the panels
 * are making their own copies and won't modify the main object
 * until they are ready to commit.
 */
MobiusConfig* ConfigEditor::getMobiusConfig()
{
    return supervisor->getMobiusConfig();
}

/**
 * Called by the ConfigPanel after it has made modifications
 * to the MobiusConfig returned by getMobiusConfig.
 */
void ConfigEditor::saveMobiusConfig()
{
    supervisor->updateMobiusConfig();
}

UIConfig* ConfigEditor::getUIConfig()
{
    return supervisor->getUIConfig();
}

void ConfigEditor::saveUIConfig()
{
    supervisor->updateUIConfig();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
