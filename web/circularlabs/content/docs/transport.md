+++
title = 'The Transport'
draft = false
summary = 'Instructions for using the Transport to synchronize tracks and sending MIDI clocks.'
+++

The *Transport* is a new feature in build 30.  It can be used in several ways, and takes the place of
the older features involving the generation of MIDI clocks to control external devices, and can
serve as an alternative to *Track Sync* for synchronizing the recording of Mobius tracks.

The Transport is not visible in the UI by default.  To see the Transport, you will need to add it
to the *Main Elements* list in the display layout.  From the *Display* menu, select *Edit Layouts*,
then under the *Main Elements* tab, drag the element named "Transport" from the right side to the left.
Click Save.  Like other UI elements, the Transport can be moved around by clicking on it and dragging it,
and it can be resized by clicking and dragging on the border.

The Transport UI consists of two rows.  The top row contains these elements:

* Location Meter (aka the "radar")
* Beat flasher
* Start/Stop button
* Tap Tempo button
* Current tempo display

The bottom row shows information about the configuration of the Transport including:

* The number of beats in one bar
* The number of bars in one loop
* The current elapsed beat
* The current elspaed bar

Pressing the *Start* button will start the transport running at the current tempo.  The location meter
will spin and the beat light will flash.  The light flashes green when it is on a beat, yellow when it on
a bar, and red when it is on a loop.  After pressing the *Start* button, the name changes to *Stop* and pressing it again stops the transport and rewinds it to the start.

If the Transport is configured to generate MIDI clocks they will be sent out to the MIDI device
configured as the "App Sync" or "Plugin Sync" device in the *Midi Devices* window.

So far, this is a common concept.  Just about every audio application has something similar.  What makes
the Transport different from the transport controls in most DAWs, is the way in which the tempo can be set and the ways in which starting and stopping it can be automated.

Talking about what the transport does and how it is used is difficult for older Mobius users.  Mobius grew up without a transport and it evolved ways to accomplish the same things using different terminology.  
For older Mobius users, the simplest way to think of the transport is as a short empty track that you
record simply for the purposes of synchronizing other tracks.  This was a common technique in the past.
This empty track would be the Out Sync Master, it would generate MIDI clocks, and the other tracks
could synchronize with it using Track Sync.

Like an empty  track, the transport has a length, this length can be converted into
a clock tempo, and the length can be divided into smaller units.  The subdivisions of a track have been
called *Cycles* and *Subcycles*.  The transport uses the more familiar terms *Bars* and *Beats*.
Unlike a track, the Transport does not record or play any audibile content, and the tempo can
be more freely controlled.

Old-school loopers have come to think of a transport as a *bad thing*.  It is something that locks you into
a tempo and forces you to make loops that are of a pre-defined size.  They much prefer "first loop capability" where you can define the length of loops on the fly using any tempo or length they choose at the time.  It is important to understand that while the Mobius transport can be used to record loops of a pre-defined tempo and size, that is only one way to use it.  When you think about it, selecting a tempo using "tap tempo" is really no different than recording a "first loop".  You are defining a span of time with two presses of a button, and once that span of time is locked in, you can choose to synchronize with it.

What makes the Mobius transport different than synching with the plugin host transport, is that the tempo of the transport can be defined by recording a live audio track.  To do that, the track is configured to be the *Transport Master*.   Once the track has finished recording, a tempo that matches the length of the track is calculated and given to the transport.  The transport will now send MIDI clocks at that tempo.  This is almost exactly the same as the old Mobius parameter *Sync Mode* to *Out*.

Once that first audio or MIDI track is recorded and becomes the transport master, you can synchronize other tracks with it using *track sync* as you did before.  As an alternative though you can choose to make the track follow the Transport for synchronization rather than another track.  The result is much the same as if you were following the transport in a host application.




