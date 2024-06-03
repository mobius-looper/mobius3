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

## Build 5 Changes

- Mac install should now support Intel hardware
- Windows install now uses static Microsoft libraries so you should no longer get errors about missing DLL files
- Right clicking on a button opens a color selection popup
- Using the **Revert** button in the Setup and Preset configuration panels no longer crashes
- Display Configuration panel has been reorganized to make it easier to use

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





