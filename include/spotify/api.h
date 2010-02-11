#ifndef LIBOPENSPOTIFY_API_H
#define LIBOPENSPOTIFY_API_H

#ifdef __cplusplus
extern "C" {
#endif


/*
 * For documentation, see Spotify's developer site
 * http://developer.spotify.com/en/libspotify/docs/
 *
 */


#include <stddef.h>


/* Current API version */
#define SPOTIFY_API_VERSION 3


/* Calling conventions */
#ifdef _WIN32
# define SP_CALLCONV __stdcall
#ifdef SPOTIFY_DLL
# define SP_LIBEXPORT(x) __declspec(dllexport) x __stdcall
#else
# define SP_LIBEXPORT(x) x __stdcall
#endif
#else
# define SP_CALLCONV
# define SP_LIBEXPORT(x) x
#endif


#ifndef __bool_true_false_are_defined
#ifdef _WIN32
#ifndef __cplusplus
#define bool unsigned char
#endif
#else
typedef unsigned char bool;
#endif
#endif
#define byte unsigned char


/* Enumerations */
typedef enum {
	SP_CONNECTION_STATE_LOGGED_OUT,
	SP_CONNECTION_STATE_LOGGED_IN,
	SP_CONNECTION_STATE_DISCONNECTED,
	SP_CONNECTION_STATE_UNDEFINED,
} sp_connectionstate;


typedef enum {
	SP_ERROR_OK,
	SP_ERROR_BAD_API_VERSION,
	SP_ERROR_API_INITIALIZATION_FAILED,
	SP_ERROR_TRACK_NOT_PLAYABLE,
	SP_ERROR_RESOURCE_NOT_LOADED,
	SP_ERROR_BAD_APPLICATION_KEY,
	SP_ERROR_BAD_USERNAME_OR_PASSWORD,
	SP_ERROR_USER_BANNED,
	SP_ERROR_UNABLE_TO_CONTACT_SERVER,
	SP_ERROR_CLIENT_TOO_OLD,
	SP_ERROR_OTHER_PERMANENT,
	SP_ERROR_BAD_USER_AGENT,
	SP_ERROR_MISSING_CALLBACK,
	SP_ERROR_INVALID_INDATA,
	SP_ERROR_INDEX_OUT_OF_RANGE,
	SP_ERROR_USER_NEEDS_PREMIUM,
	SP_ERROR_OTHER_TRANSIENT,
	SP_ERROR_IS_LOADING,
} sp_error;


typedef enum {
	SP_SAMPLETYPE_INT16_NATIVE_ENDIAN
} sp_sampletype;


typedef struct {
	sp_sampletype sample_type;
	int sample_rate;
	int channels;
} sp_audioformat;


typedef enum {
        SP_LINKTYPE_INVALID,
        SP_LINKTYPE_TRACK,
        SP_LINKTYPE_ALBUM,
        SP_LINKTYPE_ARTIST,
        SP_LINKTYPE_SEARCH,
        SP_LINKTYPE_PLAYLIST,
} sp_linktype;


typedef enum {
	SP_ALBUMTYPE_ALBUM,
	SP_ALBUMTYPE_SINGLE,
	SP_ALBUMTYPE_COMPILATION,
	SP_ALBUMTYPE_UNKNOWN,
} sp_albumtype;


typedef enum {
        SP_IMAGE_FORMAT_UNKNOWN = -1,
        SP_IMAGE_FORMAT_JPEG,
} sp_imageformat;


/* New as of libspotify 0.0.3 */
typedef enum {
	SP_TOPLIST_TYPE_ARTISTS,
	SP_TOPLIST_TYPE_ALBUMS,
	SP_TOPLIST_TYPE_TRACKS,
} sp_toplisttype;

typedef enum {
	SP_TOPLIST_REGION_EVERYWHERE,
	SP_TOPLIST_REGION_MINE,
} sp_toplistregion;



/* Typedefs forward declarations */
typedef struct sp_session sp_session;
typedef struct sp_track sp_track;
typedef struct sp_album sp_album;
typedef struct sp_artist sp_artist;
typedef struct sp_artistbrowse sp_artistbrowse;
typedef struct sp_albumbrowse sp_albumbrowse;
typedef struct sp_toplistbrowse sp_toplistbrowse;
typedef struct sp_search sp_search;
typedef struct sp_link sp_link;
typedef struct sp_image sp_image;
typedef struct sp_user sp_user;
typedef struct sp_playlist sp_playlist;
typedef struct sp_playlistcontainer sp_playlistcontainer;



/* Typedefs for open structures */
typedef struct {
	void (SP_CALLCONV *logged_in)(sp_session *session, sp_error error);
	void (SP_CALLCONV *logged_out)(sp_session *session);
	void (SP_CALLCONV *metadata_updated)(sp_session *session);
	void (SP_CALLCONV *connection_error)(sp_session *session, sp_error error);
	void (SP_CALLCONV *message_to_user)(sp_session *session, const char *message);
	void (SP_CALLCONV *notify_main_thread)(sp_session *session);
	int (SP_CALLCONV *music_delivery)(sp_session *session, const sp_audioformat *format, const void *frames, int num_frames);
	void (SP_CALLCONV *play_token_lost)(sp_session *session);
	void (SP_CALLCONV *log_message)(sp_session *session, const char *data);
	void (SP_CALLCONV *end_of_track)(sp_session *session);
} sp_session_callbacks;


typedef struct {
	int api_version;
	const char *cache_location;
	const char *settings_location;
	const void *application_key;
	size_t application_key_size;
	const char *user_agent;
	const sp_session_callbacks *callbacks;
	void *userdata;
} sp_session_config;


typedef struct {
	void (SP_CALLCONV *playlist_added)(sp_playlistcontainer *pc, sp_playlist *playlist, int position, void *userdata);
	void (SP_CALLCONV *playlist_removed)(sp_playlistcontainer *pc, sp_playlist *playlist, int position, void *userdata);
	void (SP_CALLCONV *playlist_moved)(sp_playlistcontainer *pc, sp_playlist *playlist, int position, int new_position, void *userdata);
	void (SP_CALLCONV *container_loaded)(sp_playlistcontainer *pc, void *userdata);
} sp_playlistcontainer_callbacks;


typedef struct {
	void (SP_CALLCONV *tracks_added)(sp_playlist *pl, sp_track * const *tracks, int num_tracks, int position, void *userdata);
	void (SP_CALLCONV *tracks_removed)(sp_playlist *pl, const int *tracks, int num_tracks, void *userdata);
	void (SP_CALLCONV *tracks_moved)(sp_playlist *pl, const int *tracks, int num_tracks, int new_position, void *userdata);
	void (SP_CALLCONV *playlist_renamed)(sp_playlist *pl, void *userdata);
	void (SP_CALLCONV *playlist_state_changed)(sp_playlist *pl, void *userdata);
	void (SP_CALLCONV *playlist_update_in_progress)(sp_playlist *pl, bool done, void *userdata);
	void (SP_CALLCONV *playlist_metadata_updated)(sp_playlist *pl, void *userdata);
} sp_playlist_callbacks;


/* Typedefs for callbacks */
typedef void SP_CALLCONV albumbrowse_complete_cb(sp_albumbrowse *result, void *userdata);
typedef void SP_CALLCONV artistbrowse_complete_cb(sp_artistbrowse *result, void *userdata);
typedef void SP_CALLCONV image_loaded_cb(sp_image *image, void *userdata);
typedef void SP_CALLCONV search_complete_cb(sp_search *result, void *userdata);
typedef void SP_CALLCONV toplistbrowse_complete_cb(sp_toplistbrowse *result, void *userdata);


/* API prototypes */
SP_LIBEXPORT(const char*) sp_error_message(sp_error error);

SP_LIBEXPORT(sp_error) sp_session_init(const sp_session_config *config, sp_session **sess);
SP_LIBEXPORT(sp_error) sp_session_login(sp_session *session, const char *username, const char *password);
SP_LIBEXPORT(sp_user *) sp_session_user(sp_session *session);
SP_LIBEXPORT(sp_error) sp_session_logout(sp_session *session);
SP_LIBEXPORT(sp_connectionstate) sp_session_connectionstate(sp_session *session);
SP_LIBEXPORT(void *) sp_session_userdata(sp_session *session);
SP_LIBEXPORT(void) sp_session_process_events(sp_session *session, int *next_timeout);
SP_LIBEXPORT(sp_error) sp_session_player_load(sp_session *session, sp_track *track);
SP_LIBEXPORT(sp_error) sp_session_player_play(sp_session *session, bool play);
SP_LIBEXPORT(sp_error) sp_session_player_seek(sp_session *session, int offset);
SP_LIBEXPORT(void) sp_session_player_unload(sp_session *session);
SP_LIBEXPORT(sp_playlistcontainer *) sp_session_playlistcontainer(sp_session *session);

SP_LIBEXPORT(sp_link *) sp_link_create_from_string(const char *link);
SP_LIBEXPORT(sp_link *) sp_link_create_from_track(sp_track *track, int offset);
SP_LIBEXPORT(sp_link *) sp_link_create_from_album(sp_album *album);
SP_LIBEXPORT(sp_link *) sp_link_create_from_artist(sp_artist *artist);
SP_LIBEXPORT(sp_link *) sp_link_create_from_playlist(sp_playlist *playlist);
SP_LIBEXPORT(sp_link *) sp_link_create_from_search(sp_search *search);
SP_LIBEXPORT(int) sp_link_as_string(sp_link *link, char *buffer, int buffer_size);
SP_LIBEXPORT(sp_linktype) sp_link_type(sp_link *link);
SP_LIBEXPORT(sp_track *) sp_link_as_track(sp_link *link);
SP_LIBEXPORT(sp_track *) sp_link_as_track_and_offset(sp_link *link, int *offset);
SP_LIBEXPORT(sp_album *) sp_link_as_album(sp_link *link);
SP_LIBEXPORT(sp_artist *) sp_link_as_artist(sp_link *link);
SP_LIBEXPORT(void) sp_link_add_ref(sp_link *link);
SP_LIBEXPORT(void) sp_link_release(sp_link *link);

SP_LIBEXPORT(bool) sp_track_is_loaded(sp_track *track);
SP_LIBEXPORT(sp_error) sp_track_error(sp_track *track);
SP_LIBEXPORT(bool) sp_track_is_available(sp_track *track);
SP_LIBEXPORT(bool) opensp_track_has_explicit_lyrics(sp_track *track);
SP_LIBEXPORT(const char *) sp_track_name(sp_track *track);
SP_LIBEXPORT(int) sp_track_duration(sp_track *track);
SP_LIBEXPORT(int) sp_track_popularity(sp_track *track);
SP_LIBEXPORT(int) sp_track_disc(sp_track *track);
SP_LIBEXPORT(int) sp_track_index(sp_track *track);
SP_LIBEXPORT(sp_album *) sp_track_album(sp_track *track);
SP_LIBEXPORT(int) sp_track_num_artists(sp_track *track);
SP_LIBEXPORT(sp_artist *) sp_track_artist(sp_track *track, int index);
SP_LIBEXPORT(void) sp_track_add_ref(sp_track *track);
SP_LIBEXPORT(void) sp_track_release(sp_track *track);

SP_LIBEXPORT(bool) sp_album_is_loaded(sp_album *album);
SP_LIBEXPORT(bool) sp_album_is_available(sp_album *album);
SP_LIBEXPORT(sp_albumtype) sp_album_type(sp_album *album);
SP_LIBEXPORT(sp_artist *) sp_album_artist(sp_album *album);
SP_LIBEXPORT(const byte *) sp_album_cover(sp_album *album);
SP_LIBEXPORT(const char *) sp_album_name(sp_album *album);
SP_LIBEXPORT(int) sp_album_year(sp_album *album);
SP_LIBEXPORT(void) sp_album_add_ref(sp_album *album);
SP_LIBEXPORT(void) sp_album_release(sp_album *album);

SP_LIBEXPORT(sp_albumbrowse *) sp_albumbrowse_create(sp_session *session, sp_album *album, albumbrowse_complete_cb *callback, void *userdata);
SP_LIBEXPORT(bool) sp_albumbrowse_is_loaded(sp_albumbrowse *alb);
SP_LIBEXPORT(sp_error) sp_albumbrowse_error(sp_albumbrowse *alb);
SP_LIBEXPORT(sp_album *) sp_albumbrowse_album(sp_albumbrowse *alb);
SP_LIBEXPORT(sp_artist *) sp_albumbrowse_artist(sp_albumbrowse *alb);
SP_LIBEXPORT(int) sp_albumbrowse_num_copyrights(sp_albumbrowse *alb);
SP_LIBEXPORT(const char *) sp_albumbrowse_copyright(sp_albumbrowse *alb, int index);
SP_LIBEXPORT(int) sp_albumbrowse_num_tracks(sp_albumbrowse *alb);
SP_LIBEXPORT(sp_track *) sp_albumbrowse_track(sp_albumbrowse *alb, int index);
SP_LIBEXPORT(const char *) sp_albumbrowse_review(sp_albumbrowse *alb);
SP_LIBEXPORT(void) sp_albumbrowse_add_ref(sp_albumbrowse *alb);
SP_LIBEXPORT(void) sp_albumbrowse_release(sp_albumbrowse *alb);

SP_LIBEXPORT(const char *) sp_artist_name(sp_artist *artist);
SP_LIBEXPORT(bool) sp_artist_is_loaded(sp_artist *artist);
SP_LIBEXPORT(void) sp_artist_add_ref(sp_artist *artist);
SP_LIBEXPORT(void) sp_artist_release(sp_artist *artist);

SP_LIBEXPORT(sp_artistbrowse *) sp_artistbrowse_create(sp_session *session, sp_artist *artist, artistbrowse_complete_cb *callback, void *userdata);
SP_LIBEXPORT(bool) sp_artistbrowse_is_loaded(sp_artistbrowse *arb);
SP_LIBEXPORT(sp_error) sp_artistbrowse_error(sp_artistbrowse *arb);
SP_LIBEXPORT(sp_artist *) sp_artistbrowse_artist(sp_artistbrowse *arb);
SP_LIBEXPORT(int) sp_artistbrowse_num_portraits(sp_artistbrowse *arb);
SP_LIBEXPORT(const byte *) sp_artistbrowse_portrait(sp_artistbrowse *arb, int index);
SP_LIBEXPORT(int) sp_artistbrowse_num_tracks(sp_artistbrowse *arb);
SP_LIBEXPORT(sp_track *) sp_artistbrowse_track(sp_artistbrowse *arb, int index);
SP_LIBEXPORT(int) sp_artistbrowse_num_albums(sp_artistbrowse *arb);
SP_LIBEXPORT(sp_album *) sp_artistbrowse_album(sp_artistbrowse *arb, int index);
SP_LIBEXPORT(int) sp_artistbrowse_num_similar_artists(sp_artistbrowse *arb);
SP_LIBEXPORT(sp_artist *) sp_artistbrowse_similar_artist(sp_artistbrowse *arb, int index);
SP_LIBEXPORT(const char *) sp_artistbrowse_biography(sp_artistbrowse *arb);
SP_LIBEXPORT(void) sp_artistbrowse_add_ref(sp_artistbrowse *arb);
SP_LIBEXPORT(void) sp_artistbrowse_release(sp_artistbrowse *arb);

SP_LIBEXPORT(sp_image *) sp_image_create(sp_session *session, const byte image_id[20]);
SP_LIBEXPORT(bool) sp_image_is_loaded(sp_image *image);
SP_LIBEXPORT(sp_error) sp_image_error(sp_image *image);
SP_LIBEXPORT(void) sp_image_add_load_callback(sp_image *image, image_loaded_cb *callback, void *userdata);
SP_LIBEXPORT(void) sp_image_remove_load_callback(sp_image *image, image_loaded_cb *callback, void *userdata);
SP_LIBEXPORT(const byte *) sp_image_image_id(sp_image *image);
SP_LIBEXPORT(sp_imageformat) sp_image_format(sp_image *image);
SP_LIBEXPORT(const void *) sp_image_data(sp_image *image, size_t *data_size);
SP_LIBEXPORT(void) sp_image_add_ref(sp_image *image);
SP_LIBEXPORT(void) sp_image_release(sp_image *image);

SP_LIBEXPORT(sp_search *) sp_search_create(sp_session *session, const char *query, int track_offset, int track_count, int album_offset, int album_count, int artist_offset, int artist_count, search_complete_cb *callback, void *userdata);
SP_LIBEXPORT(bool) sp_search_is_loaded(sp_search *search);
SP_LIBEXPORT(sp_error) sp_search_error(sp_search *search);
SP_LIBEXPORT(int) sp_search_num_tracks(sp_search *search);
SP_LIBEXPORT(sp_track *) sp_search_track(sp_search *search, int index);
SP_LIBEXPORT(int) sp_search_num_albums(sp_search *search);
SP_LIBEXPORT(sp_album *) sp_search_album(sp_search *search, int index);
SP_LIBEXPORT(int) sp_search_num_artists(sp_search *search);
SP_LIBEXPORT(sp_artist *) sp_search_artist(sp_search *search, int index);
SP_LIBEXPORT(const char *) sp_search_query(sp_search *search);
SP_LIBEXPORT(const char *) sp_search_did_you_mean(sp_search *search);
SP_LIBEXPORT(int) sp_search_total_tracks(sp_search *search);
SP_LIBEXPORT(void) sp_search_add_ref(sp_search *search);
SP_LIBEXPORT(void) sp_search_release(sp_search *search);

SP_LIBEXPORT(bool) sp_playlist_is_loaded(sp_playlist *playlist);
SP_LIBEXPORT(void) sp_playlist_add_callbacks(sp_playlist *playlist, sp_playlist_callbacks *callbacks, void *userdata);
SP_LIBEXPORT(void) sp_playlist_remove_callbacks(sp_playlist *playlist, sp_playlist_callbacks *callbacks, void *userdata);
SP_LIBEXPORT(int) sp_playlist_num_tracks(sp_playlist *playlist);
SP_LIBEXPORT(sp_track *) sp_playlist_track(sp_playlist *playlist, int index);
SP_LIBEXPORT(const char *) sp_playlist_name(sp_playlist *playlist);
SP_LIBEXPORT(sp_error) sp_playlist_rename(sp_playlist *playlist, const char *new_name);
SP_LIBEXPORT(sp_user *) sp_playlist_owner(sp_playlist *playlist);
SP_LIBEXPORT(bool) sp_playlist_is_collaborative(sp_playlist *playlist);
SP_LIBEXPORT(void) sp_playlist_set_collaborative(sp_playlist *playlist, bool collaborative);
SP_LIBEXPORT(bool) sp_playlist_has_pending_changes(sp_playlist *playlist);
SP_LIBEXPORT(sp_error) sp_playlist_add_tracks(sp_playlist *playlist, const sp_track **tracks, int num_tracks, int position);
SP_LIBEXPORT(sp_error) sp_playlist_remove_tracks(sp_playlist *playlist, const int *tracks, int num_tracks);
SP_LIBEXPORT(sp_error) sp_playlist_reorder_tracks(sp_playlist *playlist, const int *tracks, int num_tracks, int new_position);

SP_LIBEXPORT(void) sp_playlistcontainer_add_callbacks(sp_playlistcontainer *pc, sp_playlistcontainer_callbacks *callbacks, void *userdata);
SP_LIBEXPORT(void) sp_playlistcontainer_remove_callbacks(sp_playlistcontainer *pc, sp_playlistcontainer_callbacks *callbacks, void *userdata);
SP_LIBEXPORT(int) sp_playlistcontainer_num_playlists(sp_playlistcontainer *pc);
SP_LIBEXPORT(sp_playlist *) sp_playlistcontainer_playlist(sp_playlistcontainer *pc, int index);
SP_LIBEXPORT(sp_playlist *) sp_playlistcontainer_add_new_playlist(sp_playlistcontainer *pc, const char *name);
SP_LIBEXPORT(sp_playlist *) sp_playlistcontainer_add_playlist(sp_playlistcontainer *pc, sp_link *link);
SP_LIBEXPORT(sp_error) sp_playlistcontainer_move_playlist(sp_playlistcontainer *pc, int index, int new_position);
SP_LIBEXPORT(sp_error) sp_playlistcontainer_remove_playlist(sp_playlistcontainer *pc, int index);

SP_LIBEXPORT(bool) sp_user_is_loaded(sp_user *user);
SP_LIBEXPORT(const char *) sp_user_display_name(sp_user *user);
SP_LIBEXPORT(const char *) sp_user_canonical_name(sp_user *user);

SP_LIBEXPORT(sp_toplistbrowse *) sp_toplistbrowse_create(sp_session *session, sp_toplisttype type, sp_toplistregion region, toplistbrowse_complete_cb *callback, void *userdata);
SP_LIBEXPORT(bool) sp_toplistbrowse_is_loaded(sp_toplistbrowse *tlb);
SP_LIBEXPORT(sp_error) sp_toplistbrowse_error(sp_toplistbrowse *tlb);
SP_LIBEXPORT(int) sp_toplistbrowse_num_artists(sp_toplistbrowse *tlb);
SP_LIBEXPORT(sp_artist *) sp_toplistbrowse_artist(sp_toplistbrowse *tlb, int index);
SP_LIBEXPORT(int) sp_toplistbrowse_num_albums(sp_toplistbrowse *tlb);
SP_LIBEXPORT(sp_album *) sp_toplistbrowse_album(sp_toplistbrowse *tlb, int index);
SP_LIBEXPORT(int) sp_toplistbrowse_num_tracks(sp_toplistbrowse *tlb);
SP_LIBEXPORT(sp_track *) sp_toplistbrowse_track(sp_toplistbrowse *tlb, int index);
SP_LIBEXPORT(void) sp_toplistbrowse_add_ref(sp_toplistbrowse *tlb);
SP_LIBEXPORT(void) sp_toplistbrowse_release(sp_toplistbrowse *tlb);

#ifdef __cplusplus
}
#endif

#endif
