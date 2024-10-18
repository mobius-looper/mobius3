+++
title = 'MIDI Tracks'
draft = false
summary = 'Introduction to MIDI Tracks'
+++

## MIDI Tracks

MIDI Tracks are an emerging new feature that allow you have tracks that behave
much like audio tracks, but record and playback MIDI events.  While the goal is to have
complete function and parameter similarity between audio and MIDI tracks, the fundamental
differences beteween the two will make some things imposible, but other things possible!

MIDI Tracks are a relatively young feature that is still undergoing refinement.  I'm hoping
you try them out and offer suggestions, but it may be one of the less stable areas
of Mobius for awhile.

## Configuring

To start using MIDI tracks, you will need add one or more of them to the list of tracks
that appear at the bottom of the Mobius window.  This is not done in what are now known
as Track Setups.  Instead they make use of a new and not quite finished concept
called the *session*.  Sessions will eventually replace Setups, Presets, most Global Parameters
and a few other things.  For now, all sessions are used for is configuring MIDI tracks.

There can currently only be one session, stored in the sessions.xml file.  In the future you
may have several.

To edit the session and add MIDI tracks go to the *tracks* menu an select *Edit MIDI Tracks*.

Near the top of this panel find the field labeled *Track Count* and enter a number from 1 to 8.  It is recommended you start with a small number of tracks.

The remaining fields can be used to set synchronization options, similar to those used
with audio tracks in the Setups.

MIDI Tracks can be added and removed without needing to restart the application or the plugin.

Once you have added a few MIDI tracks, they will appear in the track strips along the bottom
of the window using a pale red color for the track number or name.

## Parameters

MIDI Tracks use the first Preset defined in the list, usually named "Default".  It is not yet
possible to change the preset used by MIDI tracks.

Very few preset and setup parameters are recognized, among the ones that are:

* Sync Source
* Track Sync Unit
* Slave Sync Unit (MIDI clock sync)
* Loop Count
* Mute Mode
* Switch Location
* Switch Duration
* Switch Quantize
* Quantize Mode
* Empty Loop Action

## Functions





