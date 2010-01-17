#include <string.h>

#include <spotify/api.h>

#include "sp_opaque.h"


SP_LIBEXPORT(const char *) sp_error_message(sp_error error) {
	char *msg;

	switch(error) {
	case SP_ERROR_OK:
		msg = "No errors encountered.";
		break;

	case SP_ERROR_BAD_API_VERSION:
		msg = "The library version targeted does not match the one you claim you support.";
		break;

	case SP_ERROR_API_INITIALIZATION_FAILED:
		msg = "Initialization of library failed - are cache locations etc. valid?";
		break;

	case SP_ERROR_TRACK_NOT_PLAYABLE:
		msg = "The track specified for playing cannot be played.";
		break;

	case SP_ERROR_RESOURCE_NOT_LOADED:
		msg = "One or several of the supplied resources is not yet loaded.";
		break;

	case SP_ERROR_BAD_APPLICATION_KEY:
		msg = "The application key is invalid.";
		break;

	case SP_ERROR_BAD_USERNAME_OR_PASSWORD:
		msg = "Login failed because of bad username and/or password.";
		break;

	case SP_ERROR_USER_BANNED:
		msg = "The specified username is banned.";
		break;

	case SP_ERROR_UNABLE_TO_CONTACT_SERVER:
		msg = "Cannot connect to the Spotify backend system.";
		break;

	case SP_ERROR_CLIENT_TOO_OLD:
		msg = "Client is too old, library will need to be updated.";
		break;

	case SP_ERROR_OTHER_PERMANENT:
		msg = "Some other error occured, and it is permanent (e.g. trying to relogin will not help).";
		break;

	case SP_ERROR_BAD_USER_AGENT:
		msg = "The user agent string is invalid or too long.";
		break;

	case SP_ERROR_MISSING_CALLBACK:
		msg = "No valid callback registered to handle events.";
		break;

	case SP_ERROR_INVALID_INDATA:
		msg = "Input data was either missing or invalid.";
		break;

	case SP_ERROR_INDEX_OUT_OF_RANGE:
		msg = "Index out of range.";
		break;

	case SP_ERROR_USER_NEEDS_PREMIUM:
		msg = "The specified user needs a premium account.";
		break;

	case SP_ERROR_OTHER_TRANSIENT:
		msg = "A transient error occured.";
		break;

	case SP_ERROR_IS_LOADING:
		msg = "The resource is currently loading.";
		break;

	default:
		msg = "Unknown error!";
	}

	return msg;
}
