+++
title = 'Downloads'
date = 2024-05-21T21:45:17-05:00
draft = false
+++

Use the following links to download installers for Mac or Windows.  Note that these are not signed and I have not gone though the registration process with Apple or Microsoft so you will get the usual scary warnings about it being from an unknown or untrusted source.  See the security comments below for how to deal with this.

The MacOS version is distributed in two different installation packages, one for modern M1+ or "Silicon" processors, and one for older Intel processors.  It is important that you download the right installer for your machine.  If the application does not run or the plugins fail to appear in the host, you most likely installed the wrong package.

Also be careful if you are running Intel host applications on a Silicon Mac using "Rosetta".  This has its own set of compatibility problems and is not recommended.

I am aware that the icon displayed on the Mac is garbled when you run the standalone application.

Build 30 included **very** significant changes from the previous build
28.  If you have been using build 28 or earlier for some
time, I recommend you proceed with caution about upgrading to any build
after 30.  While I do run through tests on every build, the chance that behavior
you have been relying on changed in build 30+ is much higher than it has been in the past.
If you are a new user that doesn't have any complex configuration, then the most recent build in the 30+ series should be fine.

For this reason I'm keeping download links for both builds.  If you decide to try the latest build
and run into a problem, you can go back to 28 until the issue is resolved.

Please read through the new documentation pages for more on the new features that have been added.

## Build 38

[Download for Windows](https://www.mobiuslooper.com/MobiusSetup-38.exe)

[Download for MacOS Silicon](https://www.mobiuslooper.com/Mobius-38.pkg)

[Download for MacOS Intel](https://www.mobiuslooper.com/MobiusIntel-38.pkg)

## Build 28

[Download for Windows](https://www.mobiuslooper.com/MobiusSetup-28.exe)

[Download for MacOS Silicon](https://www.mobiuslooper.com/Mobius-28.pkg)

[Download for MacOS Intel](https://www.mobiuslooper.com/MobiusIntel-28.pkg)

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


