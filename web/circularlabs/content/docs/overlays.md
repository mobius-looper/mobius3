+++
title = 'Using Parameter Overlays'
draft = false
+++

An **Overlay** is a collection of parameters that can be combined with the parameters
defined in the session.    Overlays replace the older concept of the **Preset**.
        
It is important to understand that using overlays is not required.  Many will find it
simpler just to set all parameters in the session.  Overlays will initially be of interest
to older Mobius users that have created Presets, using overlays can simulate the way
Presets used to work, though there are some differences.  Even if you have used Presets in
the past, and may continue to use overlays, you should consider whether you really need them
now that the session is capable of containing every parameter used by the system.

#### Upgrading Presets

When you start Mobius 34 for the first time with an existing configuration that used
Presets, all of the Presets will be automatically converted into overlays.   The *Presets*
top-level menu is now named "Overlays" and expanding it will show the names of the available
overlays which you may select.  The "Edit Overlays..." menu item brings up a window where you
can modify, add, or delete overlays.

#### Why Overlays?

Overlays normally contain a small set of parameters you want to use at the same time
briefly during a performance.  For example, temporarily changing the way the Loop Switch
functions behave, or changing the input and output ports of tracks.    Activating overlays
is fast and efficient so you may change them frequently during live performance.   This
is unlike Sessions which have many more side effects and are not meant to be changed often.

For parameters you tend to set once and keep for a long time, those are best left in the
session.


#### How Sessions and Overlays Combine

The session contains the default values for all parameters in the system.  When you *activate*
an overlay, the parameters in the overlay replace parameters defined in the session.  When
you *deactivate* an overlay the parameters return to the values they had in the session.
Only one overlay may be active in a track at a time, but each track may be using a
different overlay.

Unlike Presets, you may choose to have *no* overlays active.  In this case the parameters
all come from the session.  To stop using overlays, select "[None]" from the Overlays
menu or send a *Select Overlay* function action with no value.
















