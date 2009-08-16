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
* Error Handling	- Completed
* Session handling 	- All but sp_session_player_*() API calls and music delivery
* Links (Spotify URIs)	- All but spotify:user: (playlist) URIs
* Tracks subsystem	- Completed
* Album subsystem	- Completed
* Artist subsystem	- Completed
* Album browsing	- Completed
* Artist browsing	- Completed
* Image handling	- Partly, no image decoding is done and width/height/format can't be retrieved
* Search subsysten	- Completed
* Playlist subsystem	- Read-only (loading), no support for create/modify/remove playlists or tracks
* User handling		- Completed


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

