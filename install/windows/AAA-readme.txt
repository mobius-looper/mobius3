Fri May 31 13:03:24 2024

Windows installer notes

Files:

BlueSphere-10.ico
  An icon file with a 32x32 blue sphere.  Probably from old code.
  Used by the .iss script for SetupIconFile, not sure what that does.

MobiusInstaller.iss
  The main installation builder script.

Inno.txt
  Old notes taken when I was learning how to use Inno Setup


Run the Inno Setup application and load the Mobiusinstaller.iss file.
From the Build menu click Compile.

It creates a directory Output containing MobiusSetup.exe, this is what you
upload to circularlabs.com.

