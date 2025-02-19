+++
title = 'The Transport'
draft = false
summary = 'Instructions for using the Transport to synchronize tracks and send MIDI clocks.'
+++

The *Transport* is a new feature in build 30.   It is responsible for the generation of MIDI clocks that can be used to synchronize with external hardware devices or other applications, and is the primary mechanism for recording multiple tracks that play in synchronization with each other without drifting apart.

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
in the future.

![Transport Parameters](/docs/images/transport-parameters.png)

The most important transport parameters are:

* Default Tempo
* Beats Per Bar
* Bars Per Loop

The transport always generates beat pulses at a defined tempo.  There are several ways to define this
tempo, the session just defines the default or *starting* tempo the transport will have when you
bring up Mobius for the first time.  You may also define the tempo during performance by using **Tap Tempo** in the UI, or by recording a **Transport Master** track.  

The *Beats Per Bar* parameter defines the length of a bar in units of beats, and *Bars Per Loop* defines the length of a loop in units of bars.  The most common value for *Beats Per Bar* is 4.  The value of *Bars Per Loop* is often just 1 in which case a bar and a loop will be the same size.

#### Why "Bars Per Loop"?

For the same reason that multiple beats are almost always organized into measures or bars, multiple bars are often thought of as a larger unit of time: a 12-bar blues for example.  While the beats per bar will often stay the same from one performance to another, larger collections of bars will vary depending on the structure of the song you have in mind, or the backing track you are using.  It is not necessary to use Bars Per Loop, but you may find it convenient when recording long tracks.  If you record a track that is 2 minutes long at 4/4 time, it might conceptually be 12, 16, 24, or 32 bars long.  If you know that is what you are going to create ahead of time, you can set Bars Per Loop accordingly which can then assist in selecting synchronization points, rather than having to count bars as they play.   

#### Sending MIDI Clocks

The Transport now controls the way in which Mobius generates MIDI clocks, replacing the
older concept of *Out Sync Mode*.  Once the transport tempo is defined, and the transport is *started* MIDI clocks may or may not be generated.  If you want to generate MIDI clocks while the transport is running you must check the *Send Midi* parameter.  When this parameter is checked, a MIDI *Start* or *Continue* message is sent whenever the transport is started along with clocks at the transport's tempo.  When you stop the transport a MIDI *Stop* message is sent.

When the transport is stopped, you can choose whether or not to continue sending MIDI clocks.  It is usually benneficial to keep clocks going even while stopped so that you give external devices time to adapt to tempo changes before they are started.  Some older devices may be confused by that, in which case you can control this with the *Send Clocks When Stopped* parameter.

The *Manual Start* option is a more advanced topic that will be discussed in detail later.  If this option is checked, a MIDI *Start* message is not sent immediately when the transport is started, instead it waits for you to send it with the *MidiStart* action that may be bound to a footswitch or other trigger.

The *Min Tempo* and *Max Tempo* parameters are used when defining the transport tempo from a *Master Track*.  This is discussed later.

#### Metronome

While not available yet, the transport will support an optional metronome.  Like most metronomes it will play a short sample on each beat, bar, or loop point that the transport reaches once it has been started.  Also planned are "count in beats" which are common in other applications.  This is still under development so suggestions are welcome.

### Transport Control

While the Session contains the starting parameters for the transport,
to use it you will need to add the **Transport Element** to the user
interface.  Since this is a new feature it is not visible by default.
To add it, go to the *Display* menu and select *Edit Layouts*. Then
under the *Main Elements* tab, drag the element named "Transport" from
the right side to the left.  Click Save.  Like other UI elements, the
Transport can be moved around by clicking on it and dragging it, and
it can be resized by clicking and dragging on the border.

![Transport Element](/docs/images/transport-overview.png)

The Transport Element contains several subcomponents that, being a science nerd, I'll call "atoms".  The atoms are arranged in two rows.  The top row contains the *Loop Radar*, *Beat Flasher*, *Stop Button*, *Tap Tempo Button*, and *Tempo Display*.  The bottom row contains information about the *Beats Per Bar*, *Bars Per Loop*, the current beat within the bar, and the current bar within the loop.

You start the transport playing by pressing the **Start** button.  While playing the text inside the button changes to **Stop** and clicking on it again will stop the transport.  The **Tap** button can be used to enter a tempo by clicking on it twice.  The current transport tempo is displayed to the right of the Tap button.  The range of tempos you can enter is constrained by the **Min Tempo** and **Max Tempo** session parameters.  If you tap a tempo outside this range it will be doubled or halved until it fits within this range.

While the transport is playing the *Loop Radar* will fill with color, similar to the loop radar in a track.  As the transport crosses beat, bar, or loop boundaries, the *Beat Flasher* will flash: green on each beat, yellow on each bar, and red on each loop.

### The Transport for Old Mobius Users

Talking about what the transport does and how it is used may be difficult for older Mobius users.  Mobius grew up without a transport and it evolved ways to accomplish the same things using different terminology.  
For older users, the simplest way to think of the transport is as a short empty track that you
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

What makes the Mobius transport different than synching with the plugin host transport, is that the tempo of the transport can be defined by recording a live audio track.  To do that, the track is configured to be the *Transport Master*.

### Transport Master Tracks

To control the tempo of the transport from a track, the track must be made the *Transport Master*.  There are two ways to accomplish this, the first is by configuring the track's *Sync Source* parameter in the session.

![Transport Master Configuration](/docs/images/transport-master.png)

Most of the names in the *Sync Source* menu represent things that generate sync pulses.  The name *Master* is a special case that means that this track would like to sync with the Transport, but it wants to **control** the transport tempo rather than following it.

For old Mobius users, this is similar to the old *Sync Mode* parameter value named *Out*.

When a track using *Sync Source Master* is finished recording, the length of the track is analyzed and converted into a tempo to give to the transport.  The formula for calculating the tempo is somewhat complex, but is obvious in most cases for shorter loops where it behaves much like *tap tempo*.  For long loops of many seconds, it is influenced by the *Bars Per Loop* parameter which will divide long loops into smaller sections before deriving the tempo.  The resulting tempo will then be constrained by the *Min Tempo* and *Max Tempo* parameters in the same way as using the *Tap* button in the transport element.

When a track is the Transport Master, the transport will be started automatically as soon as the track finishes recording.  If the *Send Midi* transport parameter is also checked, this will begin generating MIDI clocks.

#### Multiple Tracks with Sync Source Master

There can only be one Transport Master track at a time.  If you have several tracks configured with *Sync Source Master*, the first one recorded will become the transport master, and the others will revert to synchronizing with the Transport itself.

This is different than old Mobius behavior where if a track used SyncMode=Out and there was already an OutSyncMaster, it would switch to using *Track Sync* instead.

#### Changing the Transport Master Track

The track that is considered the transport master may be changed at any time by
sending the *SyncMasterTransport* function to the desired track.  When that happens, the
length of the new track determines the transport tempo.  Even tracks that did not use *Sync Source Master* can request to become the transport master.

The master track may also be *disconnected* from the transport.  Disconnection may be requested by sending the *SyncMasterTransport* function to a track that is already the transport master.  In other words, the *SyncMasterTransport* function toggles the state of being the transport master.

A track may also be disconnected from the transport if the active loop is reset, or if the track switches to a loop that is empty.  

Once a the transport master track is disconnected, another master will be automatically
selected if that track is configured with a *Sync Source* of *Master* and it is changed to have an active loop that is not empty.  This may be done by re-recording an empty loop, or switching from an empty loop to a loop that is not empty.  This is called making the track "available".  Only tracks that have *Sync Source* *Master* will be automatically selected as the master.  A track that does not use *Master* can only become the master by sending it the *SyncMasterTransport* function manually.  

#### Editing the Transport Master Track

While a track is connected to the transport, changes made to the track that
alter it's size may result in a corresponding change to the transport's tempo.
Not every change to
a track will result in a transport tempo change.  If you enlarge the
track using "rounding" functions like Multiply, InstantMultiply, or
Insert, or use forms of quantization, the fundamental beat length of the track
will remain the same and the transport tempo will not change.
Examples of changes that will change the fundamental beat length are Unrounded Multiply
and Unrounded Insert without any quantization.

If you reset the loop in the master track, or switch to a loop that is empty, this
does not automatically reassign the master track and will not alter the transport
tempo.  The track enters a state of being disconnected from the transport and the transport is allowed to run at it's current tempo.  If you then switch to a non-empty loop or re-record a new loop in the master track, it will reconnect to the transport and possibly change it's tempo.

### Synchronizing With The Transport

Tracks that are not the transport master track can synchronize with the transport
by setting their *Sync Source* to *Transport*.  New recordings in those tracks will begin
and end when the transport reaches the location specified by the *Sync Unit* parameter which may be either *Beat*, *Bar*, or *Loop*.  To synchronize with the transport it must be started, either manually with the *Start* button in the transport UI or automatically when a master track is connected.

Synchronizing with the Mobius transport is very much like synchronizing with a plugin host's transport.

For more on other forms of synchronization see the [Synchronization](../synchronization) chapter.





