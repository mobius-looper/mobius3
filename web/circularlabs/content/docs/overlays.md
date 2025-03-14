+++
title = 'Using Parameter Overlays'
draft = false
+++

An **Overlay** is a collection of parameters that can be combined with the parameters
defined in the session.    Overlays replace the older concept of the **Preset**.
        
It is important to understand that using overlays is not required.  Many will find it
simpler just to set all parameters in the session.  Until you reach a point where you
feel you need what they do, I recommend you do **not** use overlays.  They
complicate how parameter behave and can make it difficult to understand why a change
you made doesn't seem to have any effect.

If you are upgrading from Mobius 2.5 or an earlier version of 3.x then you are automatically
given a set of overlays that correspond to your old *Presets*.  You can continue to use those
in much the same way as you used presets, but I encourage you to think about whether
you can stop using them and instead just use some of the new features built into the session.

#### Parameter Layers

To understand how parameter overlays work, it is helpful to think
of parameters as being organized into several **layers**.  Each layer may have any number of parameter values in it.   When a function or script needs the value of a parameter it searches through
each layer from top to bottom until it finds a value.  If a value does
not exist in one layer, it moves to the layer below it, and continues
until it finds a value.    

There is always one layer at the bottom called the *session defaults layer*.  These are the parameters you set in the *Edit Session* window under the *Default Parameters* tab.  For many uses of Mobius, this is the only layer you need.  All of the parameters you want to use are set as session defaults and shared by all tracks.  There are no other layers to think about.

Often though, you will want some tracks to use parameter values that are different than the session defaults.  The *syncMode* parameter is one of those as are *inputPort* and *outputPort*.  For example you may want the default *syncMode* to be *Host* to synchronize with the host transport, but want some of the tracks to use a *syncMode* of *None* so the recordings in those tracks can be freely created with any length.  Or you may be using a mult-channel audio interface and want each track to have a differnet *inputPort* so that each track can record a different instrument plugged into a jack on the audio interface.  Or you may want some tracks to use the *quantize* parmeter set to *loop* to execute functions when the loop reaches the end, and other tracks to not use quantization at all.

To make a track use a parameter value other than the session default, you create a *track override layer* in the *Edit Session* window.  Any parameters you add to the track override layer will be used instead of the values found in the session defaults layer.  In normal use, the track override layer will only contain a few parameters, with most parameters comming from the session default layer.

#### How Sessions layers and Overlays Combine

As mentioned, most uses of mobius will have two layers for each track.  The session defaults and the track overrides.  These are both defined in the *Edit Session* window and will persist as long as you are using that session.

The **sessionOverlay** when activated sits between the session default layer and the track override layer, and the **trackOverlay** sits above the track override layer.

If you have both overlays enabled, there are then four parameter layers
that can each contribute parameter values.

#### Why would you want overlays?

Well, you don't necessarily.  You may find that you have them if you upgraded from an eariler Mobius release, because what used to be called Presets have been converted.  THe session overlay is what used to be called the Setup Default Preset and the track overlay is what used to be called the Track Preset.

What made Presets different from Setups is that they could be channged quickly at any time.   You could start using a track with a certain set of IO ports, synchronization settings, or quantization settings.  Then in mid performace change presets and all of those parameters would change.  Overlays are the same, they can be used to make rapid changes to many parameters at the same time.

This differs from editing the session.  Sessions should be thought of as things you use for a long period of time, at least one "song" during a performance, and possibly an entire performance.  Overlays can be changed any time, many times during a song if necessary.  If you need to change more than one parameter at exactly the same time, overlays are a convenient way to do that.  They are easier to use than a script that sets multiple paraeters, or binding several MIDI buttons to different parameters and pressing multiple buttons.

#### Why do overlays converted from Presets suck?

Presets were large.  They contined around 45 parameters.  For most people, presets were almost entirely the same except for a handful of commonly changed parameters like syncMode and inputPort.  Once you had built a few Presets and wanted to start using something like *switchQuantize* set to *loop* at all times, then you would have to edit every Preset and make that change.

So after an upgrade, it is recommended that you revisit the overlays that were converted from Preset and remove all of the parameters that you don't care about, parameters that can just use the session defaults.

#### Overlay best practices

Overlays should be as small as possible.  They should only contain the parameters you need for a particular purpose.  If you aren't using one of the esoteric parameters like slipTime or muteMode, leave them out.  Let parameters you rarely or never used just be defined by the session defaults.

#### When to use the Session Overlay vs. the Track Overlay

The session overlay supplies parameters for all tracks.  Let's say you have two instruments, a keyboard and a guitar.  The keyboard is plugged into jack 1 on the audio interface and the guitar into jack 2.  These are routed into Mobius as ports 1 and 2.  The session default layer defines *inputPort* as 1 so you start recording the keyboard in all tracks.  If you now want to switch to overdubbing with guitar, you could define a session overlay with inputPort set to 2 and activate that as the session overlay.  All tracks will now receive on port 2.

Track overlays would be used when you want to make one or more tracks behave differently than the others.  Let most tracks receive parameters from the sesison defaults or the session overlay, and for only those tracks that need to be different give them a track overlay.

Again, if you are wondering why or when you would use overlays, the chances are you don't need to use them *at all*.  Overlays will become visible only for older Mobius users that have used Presets in the past.  If you are just starting out with a fresh installation there will be no overlays and you can consider whether you need them as time goes on.

### Defining Overlays

Overlays are created 