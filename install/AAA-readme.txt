Sun May 26 14:49:50 2024

This directory contains files related to the installers for Windows and MacOS.
It is not a Projucer project.

ReleaseProcess.txt has the steps to be taken to prepepare and publish new releases.

Locations.txt has information about where files are expected to be installed.

The "windows" directory contains the Windows installer and the "mac" directory
has the MacOS installer.

The "resources" directory has files that are included with the installation beyond
the application and plugin binary files.  These include the .xml configuration files,
example scripts, and a test sample.

Note that mobius.xml and uiconfig.xml are not the same as the ones at the root
of the source tree which are during development and may contain obscure settings
used for testing.  The files under resources need to simple and have a reasonable
base configuration for users to start with.

The help.xml file is not included under resources.  Since it is edited frequently
the installer will use the one at the root of the source tree.

Currently there are no example scripts but need to add some to support the
script documentation when it is written.

There is one example sample under resources/test/samples/gcloop.wav I use
mostly to test the ability to include things in the installation package
and verify that files are put in the right place and are readable.

----------------------------------------------------------------------
Distribution List
----------------------------------------------------------------------

toddreyn@gmail.com
me@bernhardwagner.net
claudiocas@hotmail.it
cllunsford@gmail.com


