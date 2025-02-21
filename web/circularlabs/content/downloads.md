+++
title = 'Downloads'
date = 2024-05-21T21:45:17-05:00
draft = false
+++

Use the following links to download installers for Mac or Windows.  Note that these are not signed and I have not gone though the registration process with Apple or Microsoft so you will get the usual scary warnings about it being from an unknown or untrusted source.  See the security comments below for how to deal with this.

The MacOS version is distributed in two different installation packages, one for modern M1+ or "Silicon" processors, and one for older Intel processors.  It is important that you download the right installer for your machine.  If the application does not run or the plugins fail to appear in the host, you most likely installed the wrong package.

Also be careful if you are running Intel host applications on a Silicon Mac using "Rosetta".  This has its own set of compatibility problems and is not recommended.

I am aware that the icon displayed on the Mac is garbled when you run the standalone application.

Build 32 included **very** significant changes from the previous build
28.  If you have been using build 28 or earlier for some
time, I recommend you proceed with caution about upgrading to build
32.  While I do run through tests on every build, the chance that behavior
you have been relying on changed in build 32 is much higher than it has been in the past.
If you are a new user that doesn't have any complex configuration, then build 32 should be fine.

For this reason I'm keeping download links for both builds.  If you decide to try 32
and run into a problem, you can go back to 28 until the issue is resolved.

Please read through the new documentation pages for more on the new features that have been added.

## Build 28

[Download for Windows](https://www.mobiuslooper.com/MobiusSetup-28.exe)

[Download for MacOS Silicon](https://www.mobiuslooper.com/Mobius-28.pkg)

[Download for MacOS Intel](https://www.mobiuslooper.com/MobiusIntel-28.pkg)

## Build 32

[Download for Windows](https://www.mobiuslooper.com/MobiusSetup-32.exe)

[Download for MacOS Silicon](https://www.mobiuslooper.com/Mobius-32.pkg)

[Download for MacOS Intel](https://www.mobiuslooper.com/MobiusIntel-32.pkg)

## Windows Security

Windows Defender may pop up and say it has "protected your PC".  In order to run the installer click "More info", then "Run Anyway".

On Windows, the standalone application will be installed in `c:\Program Files\Circular Labs\Mobius` and the VST3 plugin will be installed
in `c:\Program Files\Common Files\VST3`.   XML support files will be installed in `c:\Users\<yourname>\AppData\Local\Circular Labs\Mobius`.
Note that while the executable files are in the usual shared locations, the support files are not.  If you need to support mulitple user logins on
the same machine, you will have to reinstall for each user.

## Mac Security

The Mac may not allow you to run the .pkg installer after downloading.  First try right-clicking on the .pkg file and selecting "open".  Choose the option to allow it even though it is not signed.

If you are prompted with a warning dialog window, in the upper right there should be an icon that looks like a question mark.  Click it.  For me this brings up a help window with a link to bring up the security settings page.  Click it.  If not, bring up System Settings and navitage to Privacy & Security.  Scroll down the "Security" section, there should be an entry there with text saying that Mobius.pkg was blocked from use and a button that says "Open Anyway".  Click it.  The package installer should run.

On MacOS, the standalone application will be installed in `/Applications`, VST3 plugin will be installed in `/Library/Audio/Plug-Ins/VST3`, and the Audio Units plugin will be installed in `/Library/Audio/Plug-Ins/Components`.

The mobius.xml and other supping files are installed in `/Users/<yourname>/Library/Application Support/Circular Labs/Mobius`.  This folder will be created only when you run the application or plugin for the first time.  As with Windows, I prefer to keep the configuration XML files under the `Users` folder so that they may be more easily edited manually without file permission problems.

## Older Releases

## Build 25
- Track select functions now work with MIDI sent through the host
- Remember the last edited binding set when returning to Edit MIDI Bindings panel
- MIDI Binding editor supports an "Active" checkbox to pass MIDI events through to the engine for testing
- Preset selectors in the Setup editor refresh to show newly added Preset names
- Fix Delete in the Preset editor not working
- Release bindings: Different bindings for press and release of the same note or cc
- MIDI Channel Override when playing MIDI tracks
- Early support for "Event Scripts" that run automatically in response to track state
- MSL support for MidiOut and GetMidiDeviceId built-in functions

## Build 24
- Merge multi-track MIDI files when loading
- Fix MidiOut script command sending through host 
- Fix plugin export device not being saved on shutdown
- Fix MIDI bindings routed through the host not targeting MIDI tracks
- Add selective quantization for MIDI track functions, including Start/Stop/Pause
- Add Follow Quantize Location to quantize events to leader track boundaries
- Add LoadMidi function for MSL scripts
- Add LoadMidi bindings to load files without scripts
- Improvements to Undo/Redo in MIDI tracks
- Improvements to auto-resizing when changing leader or follower loop sizes

## Build 23
- MIDI File support with leader/follower tracks and auto-resizing
- Improved drag-and-drop into loop slots in the UI

## Build 22
- MIDI Track configuration panel now has input/output device selectors
- MIDI Tracks in the plugin may now record and play midi through the host application
- Add rudimentary input and output level monitoring for MIDI tracks
- Fix ??? mode displayed when in Overdub mode
- Fix sync state display for MIDI tracks


## Build 21
- Fix binding MIDI commands to Configuration objects (presets, setups)
- Fix binding MIDI commands to the "activePreset" parameter using object names as the binding argument
- Simplify the way control knobs look and restore the center numbers
- Improve host bar sync with odd beat numbers and truncated bars (FLStudio)

## Build 20
- Fix issue where long-press scripts didn't work until after editing the configuration
- Fix crash changing button colors with "All" and "Some"
- Alert display element and a few others can now be corner resized
- Loop Meter element can be resized to allow for more stacked events
- Remove "Alert Height" layout property
- Fix TrackGroup function for changing group assignments with bindings
- Increase maximum alert duration to infinity, and beyond
- The configured sync output device will now be used for MIDI clocks
- Reorganization of binding set selection, now with menus
- Fix calculations and trace log flood when using ManualStart with OutSync mode
- Add the concept of the Script Library with drag-and-drop file loading
- Add the Script Editor for in application script inspection
  
## Build 19
- Dispay Layouts properties for alertHeight and alertDuration
- Don't popup an alert when ReloadScripts is used from a binding
- Fix function property propagation without requiring restart
- Add Focus Locked Parameters to the GroupDefinition panel
- Add selection table for "Reset Retain" option on parameters

## Build 17
- Fix intermittent crash on plugin loading when the number of input ports is more than 2

## Build 16
- Support for multiple instances of the plugin, should fix mysterious host crashes
- Restore track groups and add selective function replication
- Fix "metalic" sounding overdubs and proper latency compensation
- allow **Sync Master MIDI** to be a bindable function to change the Out sync master

## Build 15

- Redesign host sync for stability in pattern-based hosts like FL Studio
- Fix binding panels so changes in target selector have immediate effect on binding table
- Fix dual row track strip layouts to have consistent sizes
- Put back SyncMasterTrack function
- Once some elements in the Main Elements section of the Edit Layouts panel were deselected (moving them to the right) they would disappear and could not be added back
- Fixed help text related to floating strip element selection
- Crash after adding and removing elements from the floating track strip  
- Allow more than 8 tracks in the Scope selection menus in binding panels
- Use the window size of the standalone application in the plugin editor window

