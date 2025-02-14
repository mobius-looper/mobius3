+++
title = 'The Transport'
draft = false
summary = 'Instructions for using the Transport to synchronize tracks and send MIDI clocks.'
+++

The *Transport* is a new feature in build 30.   It is responsible for the generation of MIDI clocks that can be used to synchronize with external hardware devices, or other software applications.  It is also the primary mechanism for recording multiple tracks that play in synchronization with each other without drifting apart.

What the Transport does can be difficult to talk about, depending on
your history with Mobius.  For those that are relatively new, the
Transport may seem obvious.  All DAWs have a similar concept: a tape-like
timeline with a tempo, time signature, a metronome, and control buttons like *Play*
and *Stop*.  Some of the more sophisticated hardware loopers do as
well.

For those that have used Mobius for a long time, the need for the transport may
not seem obvious.  Tracks have long been able to generate MIDI clocks and synchronize
with each other, so why would you be interested in something new that does the same thing?

I'm going to start by describing how the transport can be used from the perspective
of someone new to Mobius, without any preconcieved notions about how things were done in the past.
Later there is a section intended for older users that compares the transport with older, similar
features and explains why you might wish to use the transport instead.

### Transport Basics

Fundamentally, the transport is like a metronome.   You give it a tempo, start it, and while it
is running it will generate *pulses* or *beats* at that tempo.   You can use these beats for several
things, but mostly they are used to start and stop the recording of a track at exact times.

Simple metronomes only generate beats at a defined tempo.  But it is usually desireable to think
about music in larger units of time.  No one turns to their band and says "let's play a 48
beat blues!".  Beats are usually combined into larger units called *measures* or *bars*, and bars
are then combined into even larger units called *songs* or in our case *loops*.

The Transport allows you to define three units of time: *beat*, *bar* and *loop*.
When you create a synchronized recording you can choose to start and stop the recording
on any of these three time units.  The reordings may not all be exactly the same length but they
will all share the same *fundamental beat length*.

These concepts should be familiar to anyone that has experience using a DAW.  Synchronizing
things with the Mobius transport is very much like synchronizing with the plugin host.  It's just
that the transport lives inside Mobius and that gives us more control over how it behaves.

### Transport Parameters

The transport supports a number of parameters to define how it generates synchronization pulses.
These are currently set in the [Session](session) but other more convenient ways will be provided
in the future.  The most important transport parameters are:

* Default Tempo
* Beats Per Bar
* Bars Per Loop

The transport always generates beat pulses at a defined tempo.  There are several ways to define this
tempo, the session just defines the default or *starting* tempo the transport will have when you
bring up Mobius for the first time.

The *Beats Per Bar* parameter defines the length of a bar in units of beats, and *Bars Per Loop* defines the length of a loop in units of bars.  The most common value for *Beats Per Bar* is 4.  The value of *Bars Per Loop* is often just 1 in which case a bar and a loop will be the same size.

#### Why "Bars Per Loop"?


### Transport Control


### Recording Synchronized Tracks














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




