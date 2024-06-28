/**
 * Gradual replacement for the old ConfigEditor/ConfigPanel with structural improvements.
 *
 * A ConfigEditor is the primary component for editing complex objects such
 * as Setups, Presets, BindingSets, UIConfig, etc.  This is is where the bulk
 * of the componentry for editing configration objects is implementated.
 *
 * ConfigEditor is placed inside an abstract container called the ConfigEditorContext.
 * The context provides an outer UI shell around the editor and provides services
 * common to all editors such as a popup panel with close buttons, draggable title bar,
 * a selector for multiple objects, and a help area.
 *
 * The abstraction of the context makes the editor more self contained and allows
 * us to experiment with other ways to present editors.
 *
 * Currently the only ConfigEditorContext implementation is ConfigPanel which extends
 * BasePanel so it can be managed by PanelFactory.  I'm not entirely happy with the
 * way things are glued together here, with multiple levels of "content" objects 
 * but it's necessary due to the way top-down resized() layouts work.
 *
 * For maintainers of this code in the future, a more subtle issue is order of
 * evaluation of constructors due to subclassing and inline member objects.
 * See comments at the top of ConfigPanel.cpp for more on this.
 * The bottom line here, is that the ConfigEditor class constructor and it's
 * subclasses should do little to no work, instead waiting for a call
 * setContext().
 *
 * 
 */

#pragma once

#include <JuceHeader.h>

//////////////////////////////////////////////////////////////////////
//
// ConfigEditorContext
//
// An abstract interface that provides services to a ConfigEditor.
//
//////////////////////////////////////////////////////////////////////

/**
 * Configuration file access is provided through a pair of get/save methods.
 * 
 * getFoo returns an ACTIVE copy of the object, so the editor must not immediately
 * modify it. It must make a copy and maintain temporary editing state until the
 * Save action is requested.  saveFoo will save whatever is currently inside
 * the active object returned by getFoo.  On Save, editors will call getFoo,
 * make the pending changes, then call saveFoo.
 *
 * Adjustments to the UI of the container are made through a set of methods
 * that will be called by the ConfigEditor during construction.  This is where
 * the buttons, object selector, and help area can be customized.
 */

class ConfigEditorContext {

  public:

    virtual ~ConfigEditorContext() {};

    //////////////////////////////////////////////////////////////////////
    //
    // Constructor Callbacks
    // 
    // The methods here will be immediately called by the ConfigEditor during
    // it's construction tell the context what to display.
    //
    //////////////////////////////////////////////////////////////////////

    // instructs the context to display an object selector 
    virtual void enableObjectSelector() = 0;

    // instructs the context to display a help area of the given height
    virtual void enableHelp(int height) = 0;
    virtual class HelpArea* getHelpArea() = 0;

    // instructs the context to display a "Revert" button in addition
    // to the default Save and Cancel buttons
    virtual void enableRevert() = 0;

    // todo: do we need to be able to add arbitrary editor specific buttons?

    //////////////////////////////////////////////////////////////////////
    //
    // Runtime Callbacks
    // 
    // The methods here will be called by the ConfigEditor at runtime to
    // ask for various things.
    //
    //////////////////////////////////////////////////////////////////////
    
    // read/write the various configuration object files

    virtual class MobiusConfig* getMobiusConfig() = 0;
    virtual void saveMobiusConfig() = 0;

    virtual class UIConfig* getUIConfig() = 0;
    virtual void saveUIConfig() = 0;
    
    virtual class DeviceConfig* getDeviceConfig() = 0;
    virtual void saveDeviceConfig() = 0;
    
    // in a few cases editors need things beyond what the context provides
    virtual class Supervisor* getSupervisor() = 0;

    // diddle the object selector
    virtual void setObjectNames(juce::StringArray names) = 0;
    virtual void addObjectName(juce::String name) = 0;
    virtual void setSelectedObject(int ordinal) =  0;
    virtual int getSelectedObject() = 0;
    virtual juce::String getSelectedObjectName() =  0;
    
};

/**
 * Base class of a component that provides an editing UI for a complex
 * object.
 */
class NewConfigEditor : public juce::Component {

  protected:

    /**
     * Context object provided by the prepare() method.
     */
    ConfigEditorContext* context = nullptr;
    
  public:

    // you may wonder why we don't pass ConfigEditorContext to this constructor
    // rather than later in setContext, see comments at the top of ConfigPanel.cpp
    // for more on this
    NewConfigEditor() {}
    virtual ~NewConfigEditor() {}

    // called at a suitable time to connect the editor to it's context
    // and to ask the context for adjustments to how things are displayed
    // simple editors may not need anything beyond just saving the context
    void prepare(ConfigEditorContext* argContext) {
        context = argContext;
        prepare();
    }

    virtual void prepare() {
    }

    /**
     * Return the name to be used in the title bar of the UI surrounding the editor
     */
    virtual juce::String getTitle() = 0;

    /**
     * Instructs the editor to load the current state of an object.
     * Any pending editing state is caneled.  This is normally called
     * when displaying an editor for the first time or after it had been
     * previously saved or canceled.
     */
    virtual void load() = 0;

    /**
     * Intructs the editor to save any pending state into the target object
     * and cause it to be saved.  Once this is called the context will always
     * call load() if it wants to use the editor again.
     */
    virtual void save() = 0;

    /**
     * Instructs the editor to cancel any pending editing state and return
     * to an unloaded state.  Once this is called, the context will always call
     * load() if it wants to use the editor again.
     */
    virtual void cancel() = 0;

    /**
     * Instructs the editor to cancel any pending editing state and reload
     * the current state of the object.  The editor remains in an loaded state
     * and the context must eventuall call save() or cancel()
     *
     * When there is no object selector, this is functionally the same as load().
     * When there is an object selector, this only reloads the state
     * of the CURRENTLY SELECTED object.
     * 
     * Making this optional since not all editors may want to support revert.
     *
     * todo: not liking this, will the current vs. all revert be obvious?
     * dont' really want two different "Revert Current" and "Revert All" buttons.
     */
    virtual void revert() {};

    /**
     * Inform the editor that it is about to be made visible after being hidden.
     * If there is any pending editing state, it is retained.
     * This is where a few editors may want to register listeners to respond
     * to the environment while they are visible.
     *
     * This will only be called if the editor is currently in a hidden state.
     *
     * todo: a better name for this pair might be suspend/resume
     *
     * This method is optional.
     */
    virtual void showing() {}

    /**
     * Inform the editor that it is about to be made invisible.
     * This will only be called if the editor is currently being shown.
     * Any pending editing state must be retained.
     *
     * This method is optional.
     */
    virtual void hiding() {}

    // Object selector notifiications
    // Thes are optional and will be called only if the editor
    // calls enableObjectSelector on the context during construction

    /**
     * Inform the editor that an object has been selected.
     * The argument is the object ordinal, which is the index into
     * the array of names returned by getObjectNames.
     * The initial selection is always zero.
     *
     * All other objectSelector methods must operate on the
     * last selected object.  The editor typically remembers this
     * or it may call EditorContext::getSelectedObject
     */
    virtual void objectSelectorSelect(int ordinal) {
        (void)ordinal;
    }

    /**
     * Inform the editor that an object has been renamed.
     */
    virtual void objectSelectorRename(juce::String newName) {
        (void)newName;
    }

    /**
     * Inform the editor that an object is to be deleted.
     * The name will have been removed from the displayed list.
     * The editor must adjust internal state to remove the current
     * object and the ordinals of all objects after this one are assumed
     * to be decreased by one.
     */
    virtual void objectSelectorDelete() {}

    /**
     * Inform the editor that a new object is to be created.
     * It must add internal editing state for the new object
     * and give it an ordinal that is one higher than the current
     * length of the object list.  The object will be given a default
     * name, typically "[New]" and will usually be followed by a call
     * to objectSelectorRename after the user enters the desired name.
     */
    virtual void objectSelectorNew(juce::String name) {
        (void)name;
    }
    
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
