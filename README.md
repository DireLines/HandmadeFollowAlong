# HandmadeFollowAlong
Following along with Handmade Hero for long enough to render sprites on screen

To get the code working...

download Visual Studio Community Edition

Make this directory hierarchy:

handmade

----misc

--------shell.bat

----code

--------build.bat

--------win32_handmade.cpp

run this command:

subst W: C:[path to handmade directory]

edit shell.bat so that it contains your path to vcvarsall.bat

run shell.bat to get compiler working

run build.bat to generate build directory with exe

from code directory, run this command:

devenv ..\..\build\win32_handmade.exe

click run
