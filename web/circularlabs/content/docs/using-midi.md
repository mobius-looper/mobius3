+++
title = 'MIDI Tracks'
draft = false
summary = 'Instructions for recording and loading MIDI tracks.'
+++

MIDI Tracks are a recent feature of Mobius 3.  They allow you to record and play MIDI loops in much the same way as audio tracks.  MIDI tracks also support a number of options that audio tracks do not have, making them useful for pre-recorded drum loops or backing tracks that change their size and tempo to match audio tracks recorded live.

To start using MIDI Tracks you must add one or more of them in the session.  See the [Sessions](../sessions) document for details on configuring tracks.

Once you have MIDI tracks, you control them by sending them functions or parameter changes using *Bindings* just like audio tracks.  Many of the same functions for recording audio tracks work with MIDI tracks, these include:

* Reset
* Record
* Overdub
* Multiply
* Insert
* Replace
* Mute
* Pause
* Start
* Stop
* Loop Switch
* Undo
* Redo

In addition, MIDI tracks support parameters like *Sync Source* and *Sync Unit* to record synchronized tracks, and *Quantize Mode* to control when functions occur.

To help identify MIDI tracks, the track number in the UI will displayed in a different color than the audio track numbers.  It is also recommend that you give them names in the session.

### Configuring MIDI Devices

You must start by configuring one or more MIDI devices for Mobius to use.  Mobius has two modes of operation: as a Standalone application and as a Plugin.  The way you go about getting MIDI into and out of Mobius may be different depending on which operating mode you are using.

#### Configuring Direct MIDI Devices in Standalone Mode

When you run Mobius standalone, it must connect directly to MIDI devices.  From the *Configuration* menu select *MIDI Devices*, the MIDI Devices window will appear.  This window contains a table of all the devices that were discovered connected to your computer.  To receive MIDI from a device, click on the *Input Devices* tab and locate the MIDI device you want to use.  Check the box in the column named *App Enable*.    Assuming you want to send MIDI to a device, click the *Output Devices* tab and do the same thing in the output devices table.

There is an area at the bottom of this window where status messages are displayed when Mobius tries to connect to a device.  Depending on the MIDI device drivers you are using, you may see an error here if another application is already using this device.

#### Configuring MIDI Devices in Plugin Mode

When you run Mobius as a plugin, there are two ways for it to use MIDI.  You can open Direct MIDI Devices as is done in standalone mode, or you can let the host be in control of the devices and arrange to have MIDI messages routed through the host and into Mobius.  How this is done is a complex topic and is different for every host.  If you have ever used a synthesizer plugin, the process is similar.  The Mobius plugin has "ports" to receive and send MIDI messages, and you need to route MIDI messages into and out of those ports.

In some cases you may also want to have the Mobius plugin directly open MIDI devices and not route messages through the host.  This is especially the case if you want to generate MIDI clocks which are much more sensitive to timing irregularities than notes.  To directly open MIDI devices as a plugin, use the *Midi Devices* table and check the boxes in the "Plugin Enable" columns.  Note that a common problem is if the host is also connecting to MIDI devices, attempting to have Mobius connect to the same devices may result in an error because the MIDI device driver does not support "multi client" mode.  You will need to disable those devices in the host application in order to make direct connection in Mobius.

### Configuring MIDI Tracks to use Devices

Once you have selected the MIDI devices to connect to, you need to tell each MIDI track which of those devices to use for input and output.  This is done in the session editor.  Select the track you wish to configure in the track table, click *General* in the parameter tree and look at the *General Parameters* form:

![MIDI Track Devices](/docs/images/midi-track-devices.png)

In this example, the *Midi Input* menu is open showing the names of the devices you have configured for input.  Selecting *Any* means the track will receive from all devices that are enabled.  If Mobius was running as a plugin, you would also see the name **Host** in the list.  Choose **Host** if you want to receive MIDI messages through the host application rather than directly from a device.

Next, open the *Midi Output* menu and select the desired output device.   Unlike the input device where *Any* can be used to receive from all devices, a track can only send to one output device.

### Recording MIDI Tracks

You record MIDI tracks just like audio tracks.  First select a MIDI track by giving it *focus*. The focused track is displayed in the UI with a white box around it.  If you are using the standard bindings, press the R key on the keyboard or whatever button on your controller you have bound to the *Record* function.  The track will begin recording.  Play some notes on your keyboard, do press *Record* again.  The loop will begin playing and you should year the notes you played during recording.

Once you have a MIDI loop recorded, you can modify it using many of the same functions that are available to audio tracks including *Overdub*, *Mute*, and *Replace*.  I'm not going to outline how every function behaves, they are mostly the same as they are for audio tracks except for a few differences due to the way MIDI data works.

The most important things to understand about MIDI notes is the *envelope* and *output level*.  When a MIDI note is sent to a synthesizer, it normally starts applying an envelope which changes the sound you hear over time.  The most common of these is the ADSR envelope.  THe longer you hold a note, the longer the distance into the envelope you go.  This is commonly used to make sounds that have a sharp percussive attack, then gradually decay as the note is held.  If you record one long note into Mobius you will hear the full range of the envelope being applied by the synth.

Where MIDI tracks start differing substantially from audio tracks is what happens when you edit them.  Anything you do that splits a note or changes the beginning or end will impact how the envelope is being applied by the synth.   For example if you start with one long note, then use *Replace* to insert a gap in the middle, you will end up with two MIDI notes, and both of these will start the envelope generator over from the beginning.  Similarly, if you have a long MIDI note and you do something that trims off the first half of it, the portion of the note that remains will not sound the same as it did before, the envelope generator will start over when the start of the trimmed note is reached.

How the *output level* control behaves is a bit less obvious.  MIDI 1.0 doesn't really have the notion of an output level, at least not the way we think of them in a mixer.  There are two ways you can adjust the percieved volume of a note, using a continuous control (CC) message that is implemented as the Volume controller, or by adjusting the velocity of the note.  The problem with using the Volume CC (if your synth even implements it) is that it applies to all notes on that channel, not just the notes in the Mobius track you are changing.  Mobius attempts to simulate audio track output level by applying a reduction to the velocity of each note that is sent by the track.  This can achieve a similar effect, but it will depend entirely how you have programmed the synth to respond to note velocity.

## MIDI Track Follow Parameters

MIDI Tracks support a number of parameters to help them synchronize with other tracks.  This can
be useful for pre-recorded backing tracks you want to use with live-recorded audio tracks.

*Following* means that a track will respond to things that happen in another track.  The other
track is called the *Leader*.

When a leader track does something interesting it sends a *notification* to any follower
tracks.  If the follower tracks are configured to respond to those notificiations, the
followers will react.  

### Leader Type

Indiciates whether this track will do any form of following.  The parameter values are:

* None
* Track Number
* Track Sync Master
* Transport Master
* Focused
* Host
* MIDI In


*None* means that the track will now do any following.  It will act independently of other
tracks.

*Track Number* means that the track will follow a specific track identified by number.  This
number must be entered as the value of the *Leader Track* parameter.

*Track Sync Master* means that the track will follow whichever track is currently
designated as the Track Sync Master.  This may be used instead of *Track Number* if you want
more flexibility in specifying which track should be the leader.  You can change
the Track Sync Master at any time without editing the Session.

*Transport Master* means that the track will follow whichever track is currently
designated as the Transport Master.  This is similar to using *Track Sync Master*
except we look for the current Transport Master to determine the leader track.

*Focused* means that the track that currently has focus in the UI will be the leader.
Focus is indiciated by the white box drawn around the track strip.  This is also similer
to using *Track Sync Master* except the focused track can be changed more easily.  If you
use this option you need to remember that whenever you click on a track, or use the track
selection keys, or do anything else that selects a track, this may be changing a leader.

*Host* and *MIDI In* are special leader options because they don't actually represent
tracks, they represent *Sync Sources*.  If you use these leader types, only a few of the
follow options are meaningful.  *Follow Mute* will be used when the sync source is
stopped and *Follow Record End* will be used when the sync source is stopped.  
Following sync sources is an evolving concept so if you have ideas around this please share.

### Leader Track

When you set *Leader Type* to *Track Number* you must enter a track number for this parameter.
The follower track will only respond to events sent by that specific track.

### Leader Switch Location






### Follow Record
### Follow Record End
### Follow Size
### Follow Mute
### Follow Quantize Location

### Follower Start Muted

This parameter controls whether a track will be automatically muted as soon as it is
started after the leader track finishes recording.

Normally, when a track uses *Follow Record End* it will begin playing audibly when the
leader track finishes recording.  In some cases you might to start the follower playing
in time with the leader but have it be silent so you can control when the sound is heard.
With this option on, the follower begins playing along with the leader, but is in *Mute* mode
which you then turn off with the *Mute* function when you want it to be heard.




