/**
 * The Mobius main menu.
 *
 * Dear god, this was painful...
 *
 * I find for the most part Juce makes sense, but menus made me throw things.
 * The documentation is sparse, the examples are nil, and the forums unhelpful.
 *
 * I wanted to have the traditional top menu bar optional and allow mouse location
 * popup menus instead, or both.
 *
 * But MenuBarComponent works differently than PopupMenu, and the examples such that
 * they exist don't show much with nested submenus.
 *
 * What happens below isn't pretty and I'm sure there are better ways to accomplish
 * this, but it works well enough, and I've given this far more attention than it deserves.
 *
 * Oh, and the location of the popup depends on the mouse location, it appears to put the
 * screen into quandrants, if you are in the upper left quadrant the menu top/left is where
 * the mouse is which is what I expect.  If the lower/left quadrant the bottom/left is
 * where the mouse is.  And for the right quadrants, the menu displays to the left of the mouse
 * rather than the right.  Didn't see a way to control this and it's not so bad.
 *  
 * Ladies and gentlemen, a fucking menu bar with companion popup.
 * 
 */

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../util/Util.h"
#include "../model/MobiusConfig.h"
#include "../model/UIConfig.h"
#include "../model/Session.h"
#include "../model/Symbol.h"
#include "../model/Query.h"
#include "../model/Binding.h"

#include "../Provider.h"
#include "../Producer.h"

#include "MainWindow.h"
#include "MainMenu.h"

/**
 * Construction of the MenuBarComponent is weird.
 * Juce demos did not alloccate this with member objects
 * they used std::unique_ptr and new to make it during
 * the class constructor.  It has something to do with the component
 * wanting to use the model right away and if we implement the model
 * we're not done initializing yet or something.  I tried passing
 * {this} as a member initialization list and got an exception.
 * It works to call setModel in the constructor.
 * I think because MenuComponent starts trying to use the model right away
 * and if you set it to {this} the object isn't done being constructed yet.
 */
MainMenu::MainMenu(MainWindow* w)
{
    setName("MainMenu");

    mainWindow = w;
    
    // demo used a std::unique_ptr instead of just having a local object
    //menuBar.reset (new juce::MenuBarComponent (this));
    //addAndMakeVisible (menuBar.get());

    menuBar.setModel(this);
    addAndMakeVisible(menuBar);

    setSize(500, getPreferredHeight());
}

MainMenu::~MainMenu()
{
}

void MainMenu::resized()
{
    menuBar.setBounds(getLocalBounds());
}

/**
 * Default on Window is 24 which looks fine but may be a little thick.
 */
int MainMenu::getPreferredHeight()
{
    int height = juce::LookAndFeel::getDefaultLookAndFeel().getDefaultMenuBarHeight();
    // Trace(2, "Default menu bar height %d\n", height);
    return height;
}

//////////////////////////////////////////////////////////////////////
// 
// MenuBarModel
// 
//////////////////////////////////////////////////////////////////////

/**
 * Return top level menu bar names
 */
juce::StringArray MainMenu::getMenuBarNames()
{
    return MenuNames;
}

/**
 * Return a menu for an index.
 * 
 * It appears that the menuIndex matches the name returned by getMenuBarNames
 * so we don't need the menuName arguemnt.  Maybe if you want dynamically
 * defined bar names and won't have a fixed index?
 *
 * Unlike the Component model, menus don't use pointers much, it passes
 * object copies around.
 *
 * web on object returning
 * https://www.bogotobogo.com/cplusplus/object_returning.php#:~:text=If%20a%20method%20or%20function,a%20reference%20to%20an%20object.
 *
 * If a method or function returns a local object, it should return an object, not a reference.
 * If a method or function returns an object of a class for which there is no public copy constructor, such as ostream class, it must return a reference to an object.
 *
 * it is an error to return a pointer to a local object, so why does returning a reference work?
 * the stack is going to unwind, and without a copy constrctor, where does it go?
 * whatever the answer, PopupMenu does have a copy constrctor
 * 
 * the use of object passing rather than pointers is interesting here
 * you will pay for copying in order to get automatic memory reclamation
 * seems reasonable for smaller objects
 *
 * ahh, it's making sense now
 * popup menus are shown with the show() method which returns the command id
 * of the item selected.  Since MenuBar does it's own show internally, we won't receive
 * the item id.  Can use a callback, surely there is a listener?
 *
 * Yes, experiement with MenuBarModel.menuItemActivated
 */
juce::PopupMenu MainMenu::getMenuForIndex (int menuIndex, const juce::String& menuName)
{
    (void)menuName;
    juce::PopupMenu menu;
    
    if (menuIndex == menuIndexFile)
    {
        menu.addItem(OpenProject, "Load Project...");
        menu.addItem(SaveProject, "Save Project...");
        menu.addSeparator();
        menu.addItem(OpenLoop, "Load Loop...");
        menu.addItem(SaveLoop, "Save Loop...");
        menu.addItem(QuickSave, "Quick Save");
        menu.addSeparator();
        menu.addItem(LoadScripts, "Reload Scripts");
        menu.addItem(LoadSamples, "Reload Samples");
        menu.addItem(LoadMidi, "Load MIDI File");
        menu.addItem(AnalyzeMidi, "Analyze MIDI File");
        if (!mainWindow->getProvider()->isPlugin()) {
            menu.addSeparator();
            menu.addItem(Exit, "Exit");
        }
    }

    // continue this temporarily, but the menu needs to be gone
    else if (menuIndex == menuIndexSession)
    {
        menu.addItem(EditSession, "Edit Session...");
        menu.addItem(SessionManager, "Manage Sessions...");
        menu.addItem(ParameterSets, "Parameter Sets...");
        menu.addSeparator();
        
        Provider* provider = mainWindow->getProvider();
        Producer* producer = provider->getProducer();
        juce::String currentSessionName = provider->getSession()->getName();
        juce::StringArray sessions;
        producer->getRecentSessions(sessions);
        
        int index = 0;
        for (auto name : sessions) {
            juce::PopupMenu::Item item = juce::PopupMenu::Item(name);
            item.setID(MenuSessionOffset + index);
            if (name == currentSessionName)
              item.setTicked(true);
            menu.addItem(item);
            index++;
        }
    }
    else if (menuIndex == menuIndexOverlay)
    {
        Provider* provider = mainWindow->getProvider();
        juce::StringArray names;
        provider->getOverlayNames(names);
        int active = provider->getActiveOverlay();
        int index = 0;
        for (auto name : names) {
            juce::PopupMenu::Item item = juce::PopupMenu::Item(name).setID(MenuOverlayOffset + index);
            // active overlay ordinal is 1 based, 0 means none
            if (index == (active - 1))
              item.setTicked(true);
            menu.addItem(item);
            index++;
        }
    }
    else if (menuIndex == menuIndexDisplay) {
        menu.addItem(DisplayComponents, "Edit Layouts...");
        menu.addItem(Buttons, "Edit Buttons...");
        menu.addSeparator();
        
        Provider* provider = mainWindow->getProvider();
        UIConfig* config = provider->getUIConfig();

        // Layouts
        menu.addSectionHeader(juce::String("Layouts"));
        int index = 0;
        for (auto layout : config->layouts) {
            juce::PopupMenu::Item item = juce::PopupMenu::Item(juce::String(layout->name));
            item.setID(MenuLayoutOffset + index);
            if (layout->name == config->activeLayout)
              item.setTicked(true);
            menu.addItem(item);
            index++;
        }
        
        menu.addSeparator();

        // Buttons
        menu.addSectionHeader(juce::String("Buttons"));
        index = 0;
        for (auto buttonSet : config->buttonSets) {
            juce::PopupMenu::Item item = juce::PopupMenu::Item(juce::String(buttonSet->name));
            item.setID(MenuButtonsOffset + index);
            if (buttonSet->name == config->activeButtonSet)
              item.setTicked(true);
            menu.addItem(item);
            index++;
        }

        menu.addSeparator();
        
        // Options
        menu.addSectionHeader(juce::String("Options"));
        juce::PopupMenu::Item item = juce::PopupMenu::Item("Borders");
        item.setID(MenuOptionsBorders);
        if (config->showBorders)
          item.setTicked(true);
        menu.addItem(item);
        
        item = juce::PopupMenu::Item("Identify");
        item.setID(MenuOptionsIdentify);
        // this is a weird one because the state we're toggling isn't
        // in the UIConfig, it is transient
        // ugly dependencies
        if (provider->isIdentifyMode())
          item.setTicked(true);
        menu.addItem(item);
    }


    else if (menuIndex == menuIndexBindings) {
        // these have ids within the enumeration
        menu.addItem(MidiControl, "Edit MIDI Bindings...");
        menu.addItem(KeyboardControl, "Edit Keyboard Bindings...");
        menu.addSeparator();
        
        Provider* provider = mainWindow->getProvider();
        MobiusConfig* mconfig = provider->getOldMobiusConfig();
        UIConfig* uiconfig = provider->getUIConfig();
        BindingSet* sets = mconfig->getBindingSets();
        // first one is always active and is not displayed
        if (sets != nullptr) sets = sets->getNextBindingSet();
        if (sets != nullptr) {
            // count the numbers in each category
            int alternates = 0;
            int overlays = 0;
            for (BindingSet* set = sets ; set != nullptr ; set = set->getNextBindingSet()) {
                if (set->isOverlay())
                  overlays++;
                else
                  alternates++;
            }

            // ugh, since we want to split the list we can't use the menu id as
            // the structure ordinal, save it on the object so we can correlate, ugly
            // would be nice if we could just get the damn menu item name from the id
            // which we probably can but the Juce menu model makes me too fucking angry
            int index = 0;
            if (alternates > 0) {
                menu.addSeparator();
                menu.addSectionHeader(juce::String("Binding Sets"));
                for (BindingSet* set = sets ; set != nullptr ; set = set->getNextBindingSet()) {
                    if (!set->isOverlay()) {
                        juce::PopupMenu::Item item = juce::PopupMenu::Item(juce::String(set->getName()));
                        int itemId = MenuBindingOffset + index;
                        item.setID(itemId);
                        set->transientMenuId = itemId;
                        if (uiconfig->isActiveBindingSet(juce::String(set->getName())))
                          item.setTicked(true);
                        menu.addItem(item);
                        index++;
                    }
                }
            }
            if (overlays > 0) {
                menu.addSeparator();
                menu.addSectionHeader(juce::String("Overlays"));
                for (BindingSet* set = sets ; set != nullptr ; set = set->getNextBindingSet()) {
                    if (set->isOverlay()) {
                        juce::PopupMenu::Item item = juce::PopupMenu::Item(juce::String(set->getName()));
                        int itemId = MenuBindingOffset + index;
                        item.setID(itemId);
                        set->transientMenuId = itemId;
                        if (uiconfig->isActiveBindingSet(juce::String(set->getName())))
                          item.setTicked(true);
                        menu.addItem(item);
                        index++;
                    }
                }
            }
        }
    }

    else if (menuIndex == menuIndexConfig)
    {
        //menu.addItem(MidiControl, "MIDI Control");
        //menu.addItem(KeyboardControl, "Keyboard Control");
        //menu.addSeparator();
        menu.addItem(Properties, "Function Properties");
        menu.addItem(Groups, "Track Groups");
        menu.addItem(HostParameters, "Plugin Parameters");
        menu.addSeparator();
        menu.addItem(Samples, "Samples");
        menu.addItem(LoadSamples, "Reload Samples");
        menu.addSeparator();
        menu.addItem(MidiDevices, "MIDI Devices");
        // don't show this if we're a plugin
        if (!mainWindow->getProvider()->isPlugin())
          menu.addItem(AudioDevices, "Audio Devices");
        menu.addSeparator();
        menu.addItem(UpgradeConfig, "Upgrade Configuration");
    }
    else if (menuIndex == menuIndexScripts)
    {
        menu.addItem(LoadScripts, "Reload Scripts");
        menu.addItem(Scripts, "Script Library");
        menu.addItem(ScriptEditor, "Script Editor");
        menu.addItem(Monitor, "Script Monitor");
        menu.addItem(Console, "Console");
    }
    else if (menuIndex == menuIndexHelp)
    {
        menu.addItem(KeyBindings, "Key Bindings");
        menu.addItem(MidiBindings, "MIDI Bindings");
        menu.addItem(MidiMonitor, "MIDI Monitor");
        menu.addItem(Environment, "Environment");
        menu.addSeparator();
        menu.addItem(About, "About");
    }
    else if (menuIndex == menuIndexTest) {
        menu.addItem(TestInfo, "What is this?");
        menu.addSeparator();
        // todo: won't want to show this in released code
        juce::PopupMenu::Item item = juce::PopupMenu::Item(juce::String("Test Mode"));
        item.setID(TestMode);
        if (mainWindow->getProvider()->isTestMode())
          item.setTicked(true);
        menu.addItem(item);

        menu.addItem(SymbolTable, "Symbol Table");
        menu.addItem(TraceLog, "Trace Log");
        menu.addItem(DecacheForms, "Decache Forms");
        // this never did work right
        //menu.addItem(DiagnosticWindow, "Diagnostic Window");
    }

    return menu;
}

/**
 * MenuBarModel tells us something happened
 * Our listener provides a slightly simpler interface by
 * dispensing with the menuId.
 */
void MainMenu::menuItemSelected (int itemId, int menuId)
{
    (void)menuId;
    if (listener != nullptr)
      listener->mainMenuSelection(itemId);
}

//////////////////////////////////////////////////////////////////////
//
// Popup Menu
//
//////////////////////////////////////////////////////////////////////

/**
 * Build a standalone PopupMenu that has the same contents as
 * the MenuBarComponent and can be displayed anywhere in the window.
 *
 * There doesn't appear to be a way to make a PopupMenu with just a
 * model like MenuBarModel.  You can reuse the PopupMenus created
 * by getMenuForIndex but have to build the top level set of Items
 * to represent the things that MenuBarModel::getMenuBarNames does
 * for MenuBarComponent.
 *
 * Although we can duplicate this, it might be better for the popup to
 * be somewhat more focused, we don't really need "About" and "Help" here.
 * Especially if the main menu bar is not hidden.
 *
 * Sweet tapdancing christ, the model is obscure here and the docs suck.
 * This is unlike the Component model because most things are objects not pointers,
 * except when you get to sub menus where Item has a std::unique_ptr to one, but
 * the Item itself can't be a pointer so how the fuck does this this copy?
 * After far to much time on this, don't try to addItem(Item) a sub menu
 * use addSubMenu.
 *
 * I got leaks even when using std::unqiue_ptr on the top level menu,
 * unclear what showAsync does, it must make a copy, or something...
 * god this is weird
 *
 * Read some source code and settled on this pattern which works, time to move on.
 *
 * Example from the source code:
 *
 * void MyWidget::mouseDown (const MouseEvent& e)
 *     {
 *         PopupMenu subMenu;
 *         subMenu.addItem (1, "item 1");
 *         subMenu.addItem (2, "item 2");
 * 
 *         PopupMenu mainMenu;
 *         mainMenu.addItem (3, "item 3");
 *         mainMenu.addSubMenu ("other choices", subMenu);
 * 
 *         m.showMenuAsync (...);
 *     }
 *
 * Of course, we don't get a menuItemSelected callback since there is no
 * MenuBarModel and there is no Listener interface on PopupMenu, have
 * to use a ModalComponentManager::Callback or a std::fucntion
 *
 */
void MainMenu::showPopupMenu()
{
    // don't new this, just don't
    juce::PopupMenu topMenu;

    for (int i = 0 ; i < MenuNames.size() ; i++) {
        juce::String menuName = MenuNames[i];
        juce::PopupMenu menu = getMenuForIndex(i, menuName);
        // add it as a sub-menu, do NOT try to use Item here
        topMenu.addSubMenu(menuName, menu);
    }
    
    // show it with a callback function that sends the id back
    // to this object
    juce::PopupMenu::Options options;
    topMenu.showMenuAsync(options, [this] (int result) {popupSelection(result);});
}

/**
 * Here via the callback for showMenuAsync
 * The result value seems to be the menu item id so pass
 * it to the listener like we do for MenuBarModel.
 */
void MainMenu::popupSelection(int result)
{
    //Trace(2, "Popup menu selected %d\n", result);
    if (listener != nullptr)
      listener->mainMenuSelection(result);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
