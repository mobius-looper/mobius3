+++
title = 'Downloads'
date = 2024-05-21T21:45:17-05:00
draft = false
+++

Use the following links to download installers for Mac or Windows.  Note that these are not signed and I have not gone though the registration process with Apple or Microsoft so you will get the usual scary warnings about it being from an unknown or untrusted source.  See the security comments below for how to deal with this.

**NOTE:** We have had difficulty with the Windows download being corrupted if you simply click on it using Chrome.  If the application fails to run after downloading, return to the download page, right-click on the download link, and choose "Save link as...".   Reinstall using the new installer executable.

[Download for Windows](https://www.mobiuslooper.com/MobiusSetup.exe)

[Download for MacOS Silicon](https://www.mobiuslooper.com/Mobius.pkg)

[Download for MacOS Intel](https://www.mobiuslooper.com/MobiusIntel.pkg)

The MacOS version is distributed in two different installation packages, one for modern M1+ or "Silicon" processors, and one for older Intel processors.  It is important that you download the right installer for your machine.  If the application does not run or the plugins fail to appear in the host, you most likely installed the wrong package.

Also be careful if you are running Intel host applications on a Silicon Mac using "Rosetta".  This has its own set of compatibility problems and is not recommended.

I am aware that the icon displayed on the Mac is garbled when you run the standalone application.

## Build 13
- Fix long press with CCs
- Fix anomolies in MIDI Devices panel showing previous devices in red
- Fix MIDI clock sync problem causing immediate retrigger or "gap" in playback
- Removed Update button from binding panels, added Copy
- Make level meters green instead of red
- Loop border in the loop stack is now red when it is a loop switch destination
- Errors reading projects on Mac
- Better error handling and reporting when loading projects
- Project loading ignores old absolute paths, .wav files are assumed relative to .mob
- Add MIDI Thru device selection
- Instant parameter values may be changed by click-dragging up or down
- New Trace Log panel to watch what is happening when you do the things
- New console panel that you will one day be impressed by
  
## Build 12 
- Projects
- Single loop save/load with Quick Save
- Bug fixes for sustain scripts
- Bug fix setting colors on buttons that have action arguments
- Keep button color popup visible when it is close to the right edge
- Fix display of the focus lock button, track number/name is also a focus lock toggle
- Fix active setup switching back to the first one after editing setups
- LoadScripts and LoadSamples are now bindable functions
- Upgrader will now import old setups with track names
- Host parameters may be bound to functions and scripts
- Improved binding panels, immediate updates to the table when changing fields without **Update** button
- Bindings to scripts may now have arguments that are passed through to the script

## Windows Security

Windows Defender may pop up and say it has "protected your PC".  In order to run the installer click "More info", then "Run Anyway".

On Windows, the standalone application will be installed in `c:\Program Files\Circular Labs\Mobius` and the VST3 plugin will be installed
in `c:\Program Files\Common Files\VST3`.   XML support files will be installed in `c:\Users\<yourname>\AppData\Local\Circular Labs\Mobius`.
Note that while the executable files are in the usual shared locations, the support files are not.  If you need to support mulitple user logins on
the same machine, you will have to reinstall for each user.

## Mac Security

The Mac may not allow you to run the .pkg installer after downloading.  A dialog popups up telling you how awful it is not to get Apple's permission to do anything, in the upper right there should be an icon that looks like a question mark.  Click it.  For me this brings up a help window with a link to bring up the security settings page.  Click it.  If not, bring up System Settings and navitage to Privacy & Security.  Scroll down the "Security" section, there should be an entry there with text saying that Mobius.pkg was blocked from use and a button that says "Open Anyway".  Click it.  The package installer should run.

On MacOS, the standalone application will be installed in `/Applications`, VST3 plugin will be installed in `/Library/Audio/Plug-Ins/VST3`, and the Audio Units plugin will be installed in `/Library/Audio/Plug-Ins/Components`.

The mobius.xml and other supping files are installed in `/Users/<yourname>/Library/Application Support/Circular Labs/Mobius`.  This folder will be created only when you run the application or plugin for the first time.  As with Windows, I prefer to keep the configuration XML files under the `Users` folder so that they may be more easily edited manually without file permission problems.





