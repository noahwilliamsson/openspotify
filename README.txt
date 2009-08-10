ABOUT
=====
An attempt to write an ABI-compatible libspotify clone with
technology from the despotify project.


DEVELOPMENT
===========
The latest code can be pulled from http://github.com/noahwilliamsson/openspotify
  $ git clone git://github.com/noahwilliamsson/openspotify.git


STATUS
======
Modules (from the below link) that are implemented so far are:
http://developer.spotify.com/en/libspotify/docs/modules.html
* Error Handling
* Session handling (login, most callbacks, logout)
* Links (Spotify URIs)
* Tracks subsystem
* Album subsystem
* Artist subsystem
* Album browsing (neither browsing nor callbacks are made)
* Artist browsing (neither browsing nor callbacks are made)
* Image handling (stubs)
* Playlist subsystem (partly)
* User handling (partly done)

Currently there is no support for neither searching nor audio.


DIRECTORIES
===========
examples/
- Examples from libspotify-0.0.1.tar.gz with makefiles modified 
  to link against libopenspotify.

include/
- Only contains <spotify/api.h>

libopenspotify/
- The libspotify clone, built as a shared library on Unix
  and as a static library on Windows

openspotify-simple/
- Test client that login and logouts twice
 
win32/
- Bundled openssl and zlib headers and libraries for Windows 


