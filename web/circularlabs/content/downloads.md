+++
title = 'Downloads'
date = 2024-05-21T21:45:17-05:00
draft = false
+++

Use the following links to download installers for Mac or Windows.  Note that these are not signed and I have not gone though the registration process with Apple or Microsoft so you will get the usual scary warnings about it being from an unknown or untrusted source.  See the security comments below for how to deal with this.

**NOTE:** We have had difficulty with the Windows download being corrupted if you simply click on it using Chrome.  If the application fails to run after downloading, return to the download page, right-click on the download link, and choose "Save link as...".   Reinstall using the new installer executable.

[Download for Windows](https://www.hairycrazyants.com/MobiusSetup.exe)

[Download for MacOS](https://www.hairycrazyants.com/Mobius.pkg)

I am aware that the icon displayed on the Mac is garbled when you run the standalone application.

## Build 3 Changes

- The Audio Unit plugin is now available for the Mac
- Channel numbers in the MidiOut script statement are 0 based rather than 1 based as they were in the original code
- The script Echo statement only sends the message to the debug output stream, it will not appear in the Message element in the UI
- An outputMeter was added to the docked track strips at the bottom, this may be removed

## Build 2 Changes

- changing the track count in global parameters works after restart
- changing loop count in the preset works
- loop stack now scrolls if there are more loops than 4
- loop radar displays solid red during initial recording
- loop radar and loop meter colors track modes (blue=mute, grey=halfspeed, etc.)
- script configuration table now supports selecting multiple files and directories
- tracks may now be selected by clicking on them
- fixed handling of MIDI channels which caused MIDI commands to be ignored

The changes related to MIDI channels will require adjusting any
existing MIDI bindings to set the proper channel.  The special option "Any"
was added to mean that the message will be accepted on all channels.

## Windows Security

Windows Defender may pop up and say it has "protected your PC".  In order to run the installer click "More info", then "Run Anyway".

On Windows, the standalone application will be installed in `c:\Program Files\Circular Labs\Mobius` and the VST3 plugin will be installed
in `c:\Program Files\Common Files\VST3`.   XML support files will be installed in `c:\Users\<yourname>\AppData\Local\Circular Labs\Mobius`.
Note that while the executable files are in the usual shared locations, the support files are not.  If you need to support mulitple user logins on
the same machine, you will have to reinstall for each user.

## Mac Security

The Mac may not allow you to run the .pkg installer after downloading.  A dialog popups up telling you how awful it is not to get Apple's permission to do anything, in the upper right there should be an icon that looks like a question mark.  Click it.  For me this brings up a help window with a link to bring up the security settings page.  Click it.  If not, bring up System Settings and navitage to Privacy & Security.  Scroll down the "Security" section, there should be an entry there with text saying that Mobius.pkg was blocked from use and a button that says "Open Anyway".  Click it.  The package installer should run.

On MacOS, the standalone application will be installed in `/Applications` and the VST3 plugin will be installed in `/Library/Audio/Plug-Ins/VST3`.
Mac installation is unusual due to issues with the package builder I'm currently using.  The folder `/Users/<yourname>/Library/Application Support/Circular Labs/Mobius`
will be created only when you run the application or plugin for the first time.  As with Windows, I prefer to keep the configuration XML files under the `Users` folder so that they may be more easily edited manually without file permission problems.





