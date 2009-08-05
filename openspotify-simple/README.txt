Client for testing the functionality of openlibspotify.

Most of the code is session.c and session_ready.c from:
http://developer.spotify.com/en/libspotify/docs/examples.html



BUILDING ON WIN32
=================
Right-click on the 'openspotify-simple' project and select
'Build' or 'Rebuild' from the popup-menu.
The libopenspotify library will be built as a dependency.


BUILDING ON UNIX
================
Run 'make' and both libopenspotify and the client will be built.

If the *official* Spotify library is installed (in /usr/lib) you
might want to try to link the client with it. Running make with
libspotify=1 on the command line will link with libspotify.so.
I.e, 'make libspotify=1'


RUNNING ON WIN32
================
Open a cmd.exe console and start it using
  openspotify-simple <username> <password>

If it's built with the configuration 'Debug' selected a logfile
will be created in the current working directory as despotify.log.


RUNNING ON LINUX/BSD
====================
LD_LIBRARY_PATH=../libopenspotify ./simple <username> <password>
A file with debug output will be created as /tmp/despotify if 
built with -DDEBUG (default)


RUNNING ON MAC OS X
===================
DYLD_LIBRARY_PATH=../libopenspotify ./simple <username> <password>
A file with debug output will be created as /tmp/despotify if 
built with -DDEBUG (default)
