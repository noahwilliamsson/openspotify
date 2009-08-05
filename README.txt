ABOUT
=====
An attempt to write an ABI-compatible libspotify clone with
technology from the despotify project.

Modules (from the below link) that are implemented so far are:
http://developer.spotify.com/en/libspotify/docs/modules.html
* Session handling (login, most callbacks, logout)
* Error Handling
* Links (Spotify URIs)
* User handling (partly done)


LAYOUT
======
include/
- Only contains <spotify/api.h>

libopenspotify/
- The libspotify clone

openssl/
- A bundled OpenSSL library (0.9.8k) for building on Win32
