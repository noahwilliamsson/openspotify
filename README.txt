ABOUT
=====
An attempt to write an ABI-compatible libspotify clone with
technology from the despotify project.

Most API calls in libspotify 0.0.3 are implemented. Some have stubs.
See the STATUS topic below for more information.

Questions?
Post them to noah.williamsson at gmail.com


DEVELOPMENT
===========
The latest code can be pulled from http://github.com/noahwilliamsson/openspotify
  $ git clone git://github.com/noahwilliamsson/openspotify.git

Once checked out, the latest version can be pulled using:
  $ git pull


STATUS
======
Modules (from the below link) that are implemented so far are:
http://developer.spotify.com/en/libspotify/docs/modules.html
* Error Handling	- Completed, but likely a bit buggy
* Session handling 	- Completed
* Links (Spotify URIs)	- All but spotify:user: (playlist) URIs
* Tracks subsystem	- Completed
* Album subsystem	- Completed
* Artist subsystem	- Completed
* Album browsing	- Completed
* Artist browsing	- Completed
* Toplist browsing	- Stubs, untested
* Image handling	- Completed
* Search subsysten	- Completed
* Playlist subsystem	- Read-only (loading), no support for create/modify/remove playlists or tracks
* User handling		- Completed


BUILDING
========
Project files for Microsoft Visual Studio C++, Apple Xcode aswell as
a UNIX makefile are included. 

There are a few random notes in BUILDING.txt with additional information.


BUGS
====
* Searches don't adhere to album and artist offset and count
* No local data cache
* GC of ref counted objects could have been done better and is likely buggy
* Not all routines support multiple callbacks (different function ptrs and userdata)
* The internal thread notification/message passing part is a bit messy.. 


DIRECTORIES
===========
examples/
- Examples from libspotify-0.0.1.tar.gz with makefiles modified 
  to link against libopenspotify.

include/
- Only contains <spotify/api.h> from libspotify 0.0.3

libopenspotify/
- The libspotify clone, built as a shared library on Unix
  and with static and shared (.DLL) release and debug targets
  on Windows

openspotify-simple/
- Test code that simply logs in and out twice
- Pretty much useless and should be removed
 
win32/
- Bundled openssl and zlib headers and libraries for Windows 


ACKNOWLEDGEMENTS
================
This product includes software developed by the OpenSSL Project
for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)
