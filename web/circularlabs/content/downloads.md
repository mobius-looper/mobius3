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

**NOTE** Build 15 is available for Windows only.  A Mac build will be available early the week of August 12th.

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

## Release Notes
