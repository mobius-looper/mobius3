+++
title = 'Using Sessions'
draft = false
summary = 'Instructions for defining and using sessions to manage Mobius configuration'
+++

A **Session** is a collection of configuration settings that is saved on the file system.  When you start Mobius, a session is loaded into memory and when you shutdown Mobius the current contents of the session may be saved.  You can save any number of sessions but when you use Mobius, you are always using one *active* session.

Most audio applications have a similar concept, usually also called *sessions* or *projects* or *templates*.

If you are familar with Mobius 2.5, the Session is a replacement for the **Track Setup**, the **Preset**, and most of what were formerly called **Global Parameters**.    The session contains all of the *parameters* that control how Mobius responds to actions, and and how each track uses audio, MIDI, and synchronization services.

You would typically design several sessions to accomodate different performance styles and environments.  Often a session would be used for a single "song" within a larger performance.  Or you might use different sessions for different hardware you want to connect.  Or for differences when using Mobius standandalone and using it as a plugin.

Though not available yet, the Session is also where you will store audio and MIDI track content, replacing the Mobius 2.5 concept of the **Project**.

Sessions do **not** containg bindings or scripts.  Those are configured separately and are used across sessions.

#### Where are Sessions?

Sessions are stored in directories (folders) on the file system.  It is not currently possible to
specify where those directories are, though that is planned for future releases.  Currently they are under the user's application support directory.  For Windows users this is:

    c:/Users/<yourname>/AppData/Local/Circular Labs/Mobius/sessions

For Mac users this is:

    /Users/Library/Application Support/Circular Labs/Mobius/sessions

Within the sessions directory will be subdirectories whose names are the same as the session name.
For this reason, sessions must have names that are allowed as file system directory names.  Special characters such as / \ $ or .  are not allowed.

You may wish to make a copy of the session directories as a backup, or when changing computers.

Within each session directory there will always be a single file named "session.xml".
In future relases, when sessions are allowed to contain audio and Midi content, the .wav or .mid
files with that content will also be found here.
Other supporting files such as session-specific scripts and bindings will also be stored here.

#### Upgrading from Previous Releases

If you have been using Mobius for some time, you will probably have created several *Track Setups* and *Presets*.  When you first run the most recent Mobius release, all existing Setups are converted into Sessions, and any existing Presets are coverted into *Overlays*.  An overlay is a set of parameter values that may be placed "on top of" the parameters in the session.  Using overlays is a complex topic that is described in detail in [This Document](../overlays).

The former *Tracks* menu item has been replaced by *Sessions* and the former *Presets* menu item has been replaced by *Overlays*.  Under these menus you will see the names of the sessions and overlays much like they were in the old menus.

It should be noted that in older Mobius releases, changing a *Setup* was a relatively light weight
operation.  You could change Setups at any time, even during the middle of a song you were performing.  Sessions should be thought of as heavier things that are changed less often.  Changing a session may cause audio artifacts, particularly if the session includes audio or midi content that needs to be loaded into the tracks.  Since sessions control the order and type of each track, changing a session could cause a track that is actively playing something to be deleted.  Often changing sessions was done only to change a few parameters such as the track's input and output ports, and mixer levels.  There are other ways to do that now.

## Parameter Layers

A *parameter* is something that controls how Mobius behaves.  A parameter consists of two things, a *name* and a *value*.  One of the most frequently used parameters is named *Quantize* which may have the values *Off*, *Subcycle*, *Cycle*, and *Loop*.  Giving a parameter a value is called *assigning* or *setting* or *defining* the parameter.  In some cases we also use the term *including* a parameter when something may contain a parameter value or it may not.

The session includes almost all of the parameters in the system.  Only a very few called *system parameters* are found outside the session.  

To understand how sessions and parameters behave, it is helpful to think
of parameter values as being organized into several **layers**.  Each layer may have any number of parameter values in it.   When a function or script needs the value of a parameter it searches through
each layer from top to bottom until it finds a value.  If a value does
not exist in one layer, it moves to the layer below it, and continues
until it finds a value.    

There is always one layer at the bottom called the *session defaults layer* that contains values for all parameters.  For simple uses of Mobius, this may be the only layer you need.   All tracks share the same default parameter values.   Usually though you will want some tracks to use different parameter values than other tracks.  Each track has a collection of parameters called the *track override layer*.  When a track includes a value in it's override layer, that value is used instead of the value found in the session defaults layer.  Tracks do not need to override every parameter, only the ones that need to be different from what is in the default layer.

Beyond the session defaults and the track override layers, there are two additional layers that may be inserted into this stack of layers.  The *session overlay* and the *track overlay*.  The session overlay sits between the session defaults and the track overrides, and the track overlay sits above the track overrides.

Before we go any further, it should be stressed that there is no requirement to use session or track overlays.  In fact, it is recommended that you do **NOT** use them unless you need them.  If you are new to Mobius, ignore overlays for now.  Just set all of the parameters in the session defaults layer and where necessary override a few parameters in each track.   The only reason you might see track overlays is if you have upgraded a previous installation where all of the former *Presets* will have been converted into track overlays.  This is discussed in more detail in the [Using Parameter Overlays](../overlays) document.

As shown in the following diagram, the session defaults and session overlay is shared by all tracks, but each track may have it's own track overrides and a track overlay.

![Parameter Layers](/docs/images/parameter-layers-2.svg)

Briefly, the main reason to use either form of overlay is when you need to temporarily swap in a different
set of parameters quickly and without introducing audio artifacts which may happen if you were changing the entire session.  If you needed to change 10 different parameters at the same time, rather than writing a script,
or pushing 10 different buttons on your MIDI controller, you could collect all ten parameter values into one overlay and activate it with one button.  This is approximately how *Presets* used to work which is why Presets have been converted into track overlays.  The **problem** with presets is that they contained a great many parameter values and you had to use all of them if you were going to use a Preset.  If for example you had 5 presets being used by different tracks, and you decided that *Empty Loop Action* needed to be the same in all tracks, you would have to edit all 5 presets.  Using track overlays, you would just not include *Empty Loop Action* in your overlay and the system would always get the value from the session defaults layer.  An overlay can contain as few as one parameter or as many as all of them, it's up to you.  

Appologies if this sounds confusing, but all of this exists mostly to simulate the behavior of what used to be called *Track Setups* and *Presets* which were used by a few advanced users in older releases.  It is not necessary to understand any of this to begin using Mobius.

## Session Management

Sessions are accessed from the main menu item named unsurprisingly **Sessions**   There are two sub-items **Edit Session** and **Manage Sessions**.  *Edit Session* is where you make changes to the currently active session.  *Manage Sessions* is where you can view and organize your collection of sessions.

### Edit Session

The *Edit Session* window is organized into three tabs: *Tracks*, *Default Parameters*, and *Globals*.
The Tracks tab is where you will configure the number of tracks you want and what type (audio or MIDI) they are, and where you set track override parameters.  The *Default Parameters* tab is where you set all of the parameters in the *session defaults layer* described above.  The *Globals* tab is where you define parameters for things such as the [Transport](transport) which is shared by all tracks.

![Edit Session Window](/docs/images/session-overview.png)


#### Parameter Trees

There are lots and lots of parameters that can be set in the session.  To help locate the ones you want, parameter names are organized into a tree view which is similar to a file browser.  You will see parameter trees in a number of places in the UI and they all behave in the same way.  

![Parameter Tree](/docs/images/parameter-tree-2.png)

The tree contains a number of *category* names that you expand using the little triangles.  Within each category may be other categories, or the names of individual parameters.  When you click on a category or parameter name, a *form* is displayed to the right of the tree where you can edit the parameters in that category.

At the top of the parameter tree is the *search box*.  When you type a word in the search box, the tree will close and expand categories to display only those parameters whose names contain the word you have entered.  This can help locating a parameter whose name you might know but forget what category it is under.  Deleting the words in the search box will restore the tree to it's normal form where categories are expanded manually.

#### Parameter Forms

When you click on an item in a *parameter tree* a form is displayed to the right of the tree containing
the parameters in that category.

![Parameter Form](/docs/images/parameter-form.png)

To change the value of a parameter, click on it's current value in the parameter form.  Many parameters are edited with text input fields, in this example *Subcycles* has a text field containing the number 4.  Some parameters are edited with "combo box" menus that only allow you to select from a set of pre-defined values.  A few such as *Overdub Quantized* can only have an *on* or *off* state and are edited with checkboxes.  Click on the rounded rectangle to turn the check on and off.

In some forms you will notice that the parameter names have different colors.  The color determines whether this parameter is included in the *layer* you are editing and can be changed.  If the label color is bright, it may be edited.  If it is dark, it is not included in the layer and must be *activated* by clicking on the label.

![Parameter Form](/docs/images/parameter-form-overrides.png)

In the previous example, only the *Switch Quantize* parameter is included in this layer.  The others are not and are disabled.

If it is a pale red color, it means the parameter is being *occluded* by the presence of an overlay.  Hovering over the label will popup a tool tip that contains the name of the overlay that is occluding this parameter.  This is discussed in more detail later.  This is sometimes also referred to as "shadowing" or "hiding" the parameter.

![Parameter Form](/docs/images/parameter-form-overlay.png)

The value shown for an occluded parameter is the value that comes from the overlay so you can see
exactly what the effective value will be, which may be different than the default value defined in the session.

#### Track Table

The *Tracks* tab is where you will spend most of your time.  This is where you define how many tracks of each type you want to use, the order in which they are displayed, and set the parameters that may be different for each track in the *track overrides layer*.  On the left is the *track table*, in the center is a parameter tree, and on the right are the parameter forms.

![Edit Session Window](/docs/images/track-table.png)

The track table lists all of the currently configured tracks using the pattern:

   1:Audio:<yourname>

All tracks have a unique number starting from 1.  You cannot assign track numbers, they are defined by the order in which tracks appear in the track table.  Next to the track number is the track type.  Currently this will always be either "Audio" or "Midi", though future releases will be adding more track types.  Finally the track name is included if you have set the track name.  If the track is not named, it will display only the number and type.


Track numbers have historically been used to identify tracks in *bindings* and scripts.  This is still the case, but now that sessions contain tracks of different types which you can order however you like, identifying tracks by number can lead to confusion if you are not careful.  Bindings are not currently "inside" the session.  Bindings are global and apply to all sessions.  So a binding for track number 1 in one session may not behave the same in a different session if track number 1 isn't of the same track type (audio or midi).

In future releases, bindings will be allowed to reference tracks in different, more reliable ways.  Until then, if you want a mixture of audio and MIDI tracks, it is recommended that you follow the convention of putting all of the audio tracks first, followed by any MIDI tracks.

When a track row is highlighted, right clicking the mouse will bring up a popup menu.

![Edit Session Window](/docs/images/track-table-menu.png)

The *Add* item will add a new track, prompting you to select a type.  Once you have added tracks, you can reorder them by clicking and dragging them in the table.  If you mix tracks of different types, it is recommended that you give them names to help identify them.  A track name may be specified in two ways, using the *Rename* item in the popup menu, or by selecting the *General* category in the parameter tree and editing the *Track Name* field.

The *Delete* item deletes a single track.

The *Bulk* item provides a way to add or remove several tracks at once.  You are prompted for the number of tracks of each type.   If the number of tracks is higher than it is now, new tracks will be added after the existing tracks.  If the number of tracks is higher than it is now, tracks will be deleted.   Note that when deleting tracks in bulk, tracks with the higher numbers are removed first.  After adding or deleting tracks in bulk, all tracks are renumbered which may impact bindings.


#### Track Override Parameters

Recall that each track has a *track overrides* layer that may include parameters that override the values
found in the *session defaults* layer.  After selecting a track in the track table, a parameter tree and
form will be displayed containing only the values in the overrides layer for the selected track.

As mentioned earlier, many of the parameter labels in track forms will be dark meaning the parameter is
not being overridden and it will show the default value.  To override a parameter click on the label so
that it becomes bright and the value may be modified.

It is best to keep the number of parameters a track overrides to a minimum.  If you find that you no longer
need to override a parameter and can simply use the session default, click on the label so that it turns dark again and it will display the default value.

As you select other tracks in the track table, the tree and the forms will change to show the parameter
overrides for that track.  You may edit as many tracks as necessary, you do not need to click the *Save*
button for every track, just once when you finished editing the entire session and are ready to start
using it.

There will be slightly different parameter trees and parameter forms for Audio and MIDI tracks since not all of the parameters available for MIDI tracks apply to audio tracks and vice versa.

For details on MIDI track parameters see [Using Midi Tracks](miditracks)

### Manage Sessions

The *Manage Sessions* window allows you to view all of the sessions that have been saved
and perform a few operations on them.

![Session Manager Window](/docs/images/session-manager.png)

If this is a new installation, there will be a single session named "Default".
If you have used older versions of Mobius 3, all of the *Setup* definitions from prior builds will have been converted into sessions with the same name.   The session that is currently being used will have the word "Active" in the Status column.

The window contains one large table listing all of the saved sessions.  If you highlight a row and right click the mouse, a popup menu appears.

![Session Manager Menu](/docs/images/session-manager-menu.png)

The *Load* item will load the session from the file system and make it the active session.  In the past, Setups were relatively light-weight things, consisting of little more than a few parameter definitions.  Sessions should be thought of as heavier things since they can contain content as well as parameters.   You normally would not change sessions often, and when you do tracks that are currently playing something may be removed or replaced.

This differs from how Setups were used.  It was common to use Setups just as a convenient way to
adjust all the track volume levels, or IO ports, or sync modes on the fly during live performance.
This can still be done but once sessions contain content this can result in audio glitches.
To replicate the old Setup behavior, you should use *overlays* instead.

When you load a session from this window, it also becomes the *Startup Session* and will be automatically loaded the next time you start Mobius.  

The *Copy* item allows you to make a full copy of the session, you are prompted to enter a new unique name.  This is similar to what other systems might call **Save As**.  Once you have made a copy, you can then load and edit the session parameters without impacting the original session.

The *New* item will create a new session without copying any state from an existing session.  The session will be initialized with default parameter values and track counts.  **NOTE:** This is where many applications have the notion of a "template" that can be used as the basis for new sessions.  This is being considered but is not yet available.  If you want to make a session that looks like one you already have, use Copy.

The *Rename* item will prompt you for a new session name.  Session names are not signifant for the behavior of Mobius, they just provide a way to identify sessions designed for a particular use.

The *Delete* item will delete a saved session.   Once sessions become containers of content as well as partameters you should be careful about deleting them, since content may be lost.   Deleting the session named "Default" is not allowed.
