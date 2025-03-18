+++
title = 'The Mobius Configuration Language (MCL)'
draft = false
+++

**MCL** is a simple text language that can be used to quickly make complex configuration changes.
You can use  it as alternative to the UI for creating sessions, overlays, and bindings.  It is not the
same as the scripting language MSL.

MCL is written in text files and is executed using the menu item **Run MCL Script** item from the **File** menu.  Unlike MSL you do not bind MCL scripts to MIDI or keyboard actions, it is only run from the menu, and each script is normally only run once.

### Sections

An MCL script is line oriented, meaning that each line of the file contains a command.  A file is origanized into one or more *sections* that begin with a *section heading* line.  The section headings are:

* session
* overlay
* binding

### Defining Sessions

To create or modify a session, add a session section with a line similar to this:

    session MySession

The word following the *session* keyword is the name of the session you want to create or modify.
If the session name contains spaces, it must be surrounded with double quotes.

    session "My Session"

You may also use the special name "active" which means you will modify whatever session
is currently active in memory.

    session active

If a session with the name you provide does not exist, a new session is created.  If a session
with that name already exists, it is modified.  Creating or modifying a session does not *load*
the session unless that session is already active.

After the "session" header line will be one or more parameter assignment lines of this form:

    <parameter name> <value>

For example:

    quantize cycle
    switchQuantize loop
    inputPort 1
    outputPort 2

You may have any number of parameter assignment lines following the session header.  Parameter
assignments made immediately after the session header are the *default parameters*.  These will be
the values shared by all tracks unless those tracks override the default values.

If you want a track to have a different parameter value than the default, you add a *track header*
line like this:

    track 2

After the track header, you then put other parameter assignment lines, but these parameters
only apply to track number 2.  In this example, a session is created with the default input port
of 1 but this is overridden in track 2 to use inport port number 3.

    session MySession
    inputPort 1
    track 2
    inputPort 3

To set parameter overrides for other tracks, just add more track header lines.

    track 3
    inputPort 4
    track 4
    inputPort 5

You can completely define all parameters for a session in this way, or you can just make changes
to a few parameters.  If a track currently has a parameter override that you would like to remove
and have that track use the default parameter instead, use the special name "remove" as the
parameter value.

    track 3
    inputPort remove

#### Track Scoped Lines

As an alternative to using the **track** subsection header to define track-specific parameters,
you may also give each parameter assignment line a prefix.  The prefix is a track number followed
by a colon.

    4:inputPort 2

The previous line sets the inputPort parameter to 2 in track 4.  It is the same as writing this:

    track 4
    inputPort 2

### Configuring Sync Sources

MCL can also be used to set parameters related to the synchronization sources.  Since these are
not related to tracks, they are set under the *session* header line.

    session MySession
    transportTempo 90
    transportBeatsPerBar 5
    midiBeatsPerBar 3
    hostBarOverride true
    hostBeatsPerBar 6

When assigning parameters, you will need to use their *internal names*.  These are the same as
the names you would use in an MSL or MOS script.  They are usually the same as the names displayed
in the UI except that spaces are removed, the first letter is in lowercase, and each word is capitalized.  For example, in the UI, parameter names are displayed with spaces between words and capital letters on each word:

* Switch Quantize
* Sync Source
* Track Sync Unit

The internal script names for those parameters is:

* switchQuantize
* syncSource
* trackSyncUnit

### Defining Overlays

You define overlays in the much the same way as you define sessions.  The only difference is
that the section header begins with **overlay** instead of **session**.

    overlay MyOverlay
    quantize off
    outputPort 5

### Defining Binding Sets

Using MCL can make entering a large number of MIDI or keyboard bindings much easier than using the UI.
Bindings are created or modified within a *binding set* which has a name.  To modify a binding set,
create a bindings section in the MCL file using the section header "bindings" followed by the set name.

    bindings MyBindingSet

If you are not using multiple binding sets, all bindings are stored in the standard binding
set named "Base" that you may modify with this section header.

    bindings Base

After the *bindings* section header are one or more lines that define the bindings you wish to create.  Unlike parameter assignments, bindings are complex and consist of the following values.

* type (note, control, program, key)
* channel (midi channel number from 1 to 16)
* value (midi note number, cc number, program number, key code)
* scope (track number or group name)
* symbol (target function or parameter name)
* arguments (arguments to a function)

There are several ways to specify all of these values.  The first is to use *keyword arguments*
where each value is preceeded by the name of the value you are setting.

    type note channel 1 value 42 scope 2 symbol Record

To reduce the amount of typing when creating a large number of bindings, you may specify a
*column declaration* that tells the MCL reader what the values in each line mean:

    bindings Base
    columns type channel value symbol
    note 3 42 Record
    note 3 44 Overdub

This creates two bindings, one for the *Record* function which is bound to the MIDI note number 42 on channel 3, and one for the *Overdub* function which is bound to the note number 44 also on channel 3.

Since repeating the type and channel for MIDI bindings is common, they can be defaulted by specifying
a *default declaration*.

    bindings Base
    defaults type note channel 3
    columns value symbol
    42 Record
    44 Overdub

You can change the defaults and the columns any time within a binding section.  If you wanted
to start entering bindings for channel 4 instead, just add another *defaults* line.

    defaults channel 4
    42 Multiply
    44 Insert

The most recent *columns* specification is still being used, and the default for *type* is
still *note*, but the default *channel* is now 4 instead of 3.

You may also override the defaults on any binding line by using keyword arguments, but
these must go after the values for the declared columns.

    defaults type note channel 3
    columns value symbol
    42 Record
    42 Overdub channel 4

Here the same note number 42 is used for both the *Record* and *Overdub* functions, but *Overdub*
is bound to channel 4 instead of 3.  It is the same as writing these lines, but saves one line of text.

    defaults type note channel 3
    columns value symbol
    42 Record
    defaults channel 4
    42 Overdub











