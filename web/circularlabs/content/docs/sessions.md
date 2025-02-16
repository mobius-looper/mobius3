+++
title = 'Using Sessions'
draft = false
summary = 'Instructions for defining and using sessions to manage Mobius configuration'
+++

A **Session** is a collection of configuration settings and pre-defined audio and MIDI content that is saved on the file system.  When you start Mobius, a session is *loaded* into memory and when you shutdown Mobius the current contents of the session may be *saved*.  You can save any number of sessions but when you use Mobius, you are always using one *active* session.

Most audio applications have a similar concept, usually also called *sessions* or *projects* or *templates*.

If you are familar with Mobius 2.5, the Session is essentially a replacement for the **Setup**.  It also includes some of what was once defined in **Presets** and will eventually evolve to replace **Projects** which is where audio content was stored.

## Session Management

Sessions are accessed from the main menu item named unsurprisingly "Sessions".   There are two sub-items "Edit Session" and "Manage Sessions".  *Edit Session* is where you make changes to the currently loaded session.  *Manage Sessions* is where you can view and organize your collection of sessions.

These are both very new features and the user interface is not entirely to my liking, so I'm avoiding screen shots here since I expect this to evolve.  Feel free to offer suggestions on the forum.  As usual, I go for functionality first, and worry about making it pretty later.

### Edit Session

The *Edit Session* window is organized into two tabs: *Globals* and *Tracks*.

![Edit Session Window](/docs/images/session-global-sync.png)

The *Globals* tab is where you will find configuration parameters that apply to the entire application rather than individual tracks.  It takes the place of what used to be called the *Global Parameters* window and contains new options for the [Transport](transport).

The parameters are organized into "forms" or "pages", which are listed in a column on the left side.  There are three pages: *Sync*, *Ports*, and *Miscellany*.  When you click on the name of a page, it is displayed on the right side and contains editing fields for each of the parameters.  Note that in general, changes you make in any of the popup windows do not actually take effect until you click the *Save* button at the bottom.  If you click *Cancel* the changes are discarded and nothing happens.

The *Sync* page has many new parameters for controlling how synchronization is performed using
the new [Transport](transport), the plugin host, and external MIDI clocks.

The other pages have more obscure parameters that are less interesting for most users.  These are covered in detail in the [Parameter Glossary](parameters).

The *Tracks* tab is where you will spend most of your time.  This is where you define how many tracks of each type you want to use, the order in which they are displayed, and set the parameters that may be different for each track.  On the left is the *track table*, in the center is the list of parameter pages, and on the right is the form containing the parameters for the selected page.

#### Track Table

The track table lists all of the currently configured tracks using the pattern:

    1:Audio:MyName

All tracks have a unique number starting from 1.  You cannot assign track numbers, they are defined by the order in which tracks appear in the track table.  Next to the track number is the track type.  Currently this will always be either "Audio" or "Midi", though future releases will be adding more track types.  Finally the track name is included if you have set the track name.  If the track is not named, it will display only the number and type.


Track numbers have historically been used to identify tracks in *bindings* and scripts.  This is still the case, but now that sessions contain tracks of different types which you can order however you like, identifying tracks by number can lead to confusion if you are not careful.  Bindings are not currently "inside" the session.  Bindings are global and apply to all sessions.  So a binding for track number 1 in one session may not behave the same in a different session if track number 1 isn't of the same track type (audio or midi).

In future releases, bindings will be allowed to reference tracks in different, more reliable ways.  Until then, if you want a mixture of audio and MIDI tracks, it is recommended that you follow the convention of putting all of the audio tracks first, followed by any MIDI tracks.

When a track row is highlighted, right clicking the mouse will bring up a popup menu with these items:

  * Add
  * Delete
  * Bulk

The *Add* item will add a new track, prompting you to select a type.  Once you have added tracks, you can reorder them by clicking and dragging them in the table.  To give a track a name, enter one in the Track Name field of the General page.  If all tracks have the same type and no names, it can be unclear whether reordering had any effect.

The *Bulk* item provides a way to add or remove several tracks at once.  You are prompted for the number of tracks of each type.   If the number of tracks is higher than it is now, new tracks will be added, if it is lower tracks will be deleted.   Note that when deleting tracks in bulk, tracks with the higher numbers are removed first.

The *Delete* item deletes a single track.

#### Track Parameters

To edit the parameters for a track, you first select the row in the track table, then select one of the form pages, then modify parameter fields in the form.  You can make changes to as many tracks and parameter pages as necessary, they are all held in memory until you click the *Save* button at the bottom.  You do not need to click *Save* for every page or track you want to edit, just once when you are finished and ready to make the changes available for use.

There will be different parameter pages for different track types.

Most of the parameters for audio tracks should be familar to people that previously used *Setups*.
The parameter pages for Midi tracks replaces the older prototype "Edit Midi Tracks" window.  The same parameters are there but are presented differently.

For details on each parameter consult the [Parameter Glossary](parameters) 

For details on MIDI track parameters see [Using Midi Tracks](miditracks)

Note that the various "follower" options available for MIDI tracks are not yet available
to audio tracks, but this is planned for future releases.

### Manage Sessions

The *Manage Sessions* window allows you to view all of the sessions that have been saved
and perform a few operations on them.

If this is a new installation, there will be a single session named "Default".
If you have used older versions of Mobius 3, all of the *Setup* definitions from prior builds will have been converted into sessions with the same name.   The session that is currently being used will have the word "Active" in the Status column.

The window contains one large table listing all of the saved sessions.  If you highlight a row and right click the mouse, a popup menu appears with these items:

    * Load
    * Copy
    * New
    * Rename
    * Delete

The *Load* item will load the session from the file system and make it the active session.  In the past, Setups were relatively light weight things, consisting of little more than a few parameter definitions.  Sessions should be thought of as heavier things since they can contain content as well as parameters.   You normally would not change sessions often, and when you do tracks that are currently playing something may be removed or replaced.

This differs from how Setups were used.  It was common to use Setups just as a convenient way to
adjust all the track volume levels, or IO ports, or sync modes on the fly during live performance.
This can still be done but once sessions contain content this can result in audio glitches.
To replicate the old Setup behavior, there is a new concept called *Parameter Sets* that is still
under development.

When you load a session from this window, it also becomes the *Startup Session* and will be automatically loaded the next time you start Mobius.  

The *Copy* item allows you to make a full copy of the session, you are prompted to enter a new unique name.  This is similar to what other systems might call "Save As".  Once you have made a copy, you can then load and edit the session parameters without impacting the original session.

The *New* item will create a new session without copying any state from an existing session.  The session will be initialized with default parameter values and track counts.  **NOTE:** This is where many applications have the notion of a "template" that can be used as the basis for new sessions.  This is being considered but is not yet available.  If you want to make a session that looks like one you already have, use Copy.

The *Rename* item will prompt you for a new session name.  Session names are not signifant for the behavior of Mobius, they just provide a way to identify sessions designed for a particular use.

The *Delete* item will delete a saved session.   Once sessions become containers of content as well as partameters you should be careful about deleting them, since content may be lost.   Deleting the session named "Default" is not allowed.

#### Where are Sessions?

Sessions are stored in directories (folders) on the file system.  It is not currently possible to
specify where those directories are, though that is planned for future releases.  Currently they are under the user's application support directory.  For Windows users this is:

    c:/Users/<yourname>/AppData/Local/Circular Labs/Mobius/sessions

For Mac users this is:

    /Users/Library/Application Support/Circular Labs/Mobius/sessions

Within the sessions directory will be subdirectories whose names are the same as the session name.
For this reason, sessions must have names that are allowed as file system directory names.  Special characters such as / \ $ or .  are not allowed.

Within each session directory there will always be a single file named "session.xml".

In future relases, when sessions are allowed to contain audio and Midi content, the .wav or .mid
files with that content will also be found here.

Other supporting files such as session-specific scripts and bindings may appear here as well.




       

