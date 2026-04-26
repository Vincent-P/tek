#ifndef STEAMAPIC_H
#define STEAMAPIC_H

#define S_API __declspec( dllimport )
#define S_CALLTYPE __cdecl

#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
// The 32-bit version of gcc has the alignment requirement for uint64 and double set to
// 4 meaning that even with #pragma pack(8) these types will only be four-byte aligned.
// The 64-bit version of gcc has the alignment requirement for these types set to
// 8 meaning that unless we use #pragma pack(4) our structures will get bigger.
// The 64-bit structure packing has to match the 32-bit structure packing for each platform.
#define VALVE_CALLBACK_PACK_SMALL
#else
#define VALVE_CALLBACK_PACK_LARGE
#endif

// -- Basic typedefs and constants

typedef uint32_t HSteamNetConnection;
typedef uint32_t HSteamListenSocket;
typedef int64_t SteamNetworkingMicroseconds;
typedef uint32_t SteamNetworkingPOPID;
typedef struct ISteamNetworkingUtils ISteamNetworkingUtils;
typedef struct ISteamUser ISteamUser;
typedef uint64_t uint64_steamid;
typedef int32_t HSteamUser;
typedef int32_t HSteamPipe;
#ifndef bool
#define bool  _Bool
#define false 0
#define true  1
#endif
typedef uint64_t SteamAPICall_t;
const SteamAPICall_t k_uAPICallInvalid = 0x0;

enum { k_cchMaxSteamErrMsg = 1024 };
typedef char SteamErrMsg[ k_cchMaxSteamErrMsg ];
enum SteamCallbacksConstants
{
	k_iSteamMatchmakingCallbacks = 500,
	k_iSteamMatchmakingLobbyInviteCallback = k_iSteamMatchmakingCallbacks + 3,
	k_iSteamMatchmakingLobbyEnterCallback = k_iSteamMatchmakingCallbacks + 4,
	k_iSteamMatchmakingLobbyCreatedCallback = k_iSteamMatchmakingCallbacks + 13,

	k_iSteamNetworkingMessagesCallbacks = 1250,
	k_iSteamNetworkingMessagesSessionRequestCallback = k_iSteamNetworkingMessagesCallbacks + 1,
	k_iSteamNetworkingSteamNetworkingMessagesSessionFailedCallback = k_iSteamNetworkingMessagesCallbacks + 2,
};
enum SteamNetworkingSendConstants
{
	k_nSteamNetworkingSend_Unreliable = 0,
	k_nSteamNetworkingSend_NoNagle = 1,
	k_nSteamNetworkingSend_UnreliableNoNagle = k_nSteamNetworkingSend_Unreliable|k_nSteamNetworkingSend_NoNagle,
	k_nSteamNetworkingSend_NoDelay = 4,
	k_nSteamNetworkingSend_UnreliableNoDelay = k_nSteamNetworkingSend_Unreliable|k_nSteamNetworkingSend_NoDelay|k_nSteamNetworkingSend_NoNagle,
	k_nSteamNetworkingSend_Reliable = 8,
	k_nSteamNetworkingSend_ReliableNoNagle = k_nSteamNetworkingSend_Reliable|k_nSteamNetworkingSend_NoNagle,
	k_nSteamNetworkingSend_UseCurrentThread = 16,
	k_nSteamNetworkingSend_AutoRestartBrokenSession = 32,
};


// -- Enums

enum EResult
{
	k_EResultNone = 0,							// no result
	k_EResultOK	= 1,							// success
	k_EResultFail = 2,							// generic failure
	k_EResultNoConnection = 3,					// no/failed network connection
//	k_EResultNoConnectionRetry = 4,				// OBSOLETE - removed
	k_EResultInvalidPassword = 5,				// password/ticket is invalid
	k_EResultLoggedInElsewhere = 6,				// same user logged in elsewhere
	k_EResultInvalidProtocolVer = 7,			// protocol version is incorrect
	k_EResultInvalidParam = 8,					// a parameter is incorrect
	k_EResultFileNotFound = 9,					// file was not found
	k_EResultBusy = 10,							// called method busy - action not taken
	k_EResultInvalidState = 11,					// called object was in an invalid state
	k_EResultInvalidName = 12,					// name is invalid
	k_EResultInvalidEmail = 13,					// email is invalid
	k_EResultDuplicateName = 14,				// name is not unique
	k_EResultAccessDenied = 15,					// access is denied
	k_EResultTimeout = 16,						// operation timed out
	k_EResultBanned = 17,						// VAC2 banned
	k_EResultAccountNotFound = 18,				// account not found
	k_EResultInvalidSteamID = 19,				// steamID is invalid
	k_EResultServiceUnavailable = 20,			// The requested service is currently unavailable
	k_EResultNotLoggedOn = 21,					// The user is not logged on
	k_EResultPending = 22,						// Request is pending (may be in process, or waiting on third party)
	k_EResultEncryptionFailure = 23,			// Encryption or Decryption failed
	k_EResultInsufficientPrivilege = 24,		// Insufficient privilege
	k_EResultLimitExceeded = 25,				// Too much of a good thing
	k_EResultRevoked = 26,						// Access has been revoked (used for revoked guest passes)
	k_EResultExpired = 27,						// License/Guest pass the user is trying to access is expired
	k_EResultAlreadyRedeemed = 28,				// Guest pass has already been redeemed by account, cannot be acked again
	k_EResultDuplicateRequest = 29,				// The request is a duplicate and the action has already occurred in the past, ignored this time
	k_EResultAlreadyOwned = 30,					// All the games in this guest pass redemption request are already owned by the user
	k_EResultIPNotFound = 31,					// IP address not found
	k_EResultPersistFailed = 32,				// failed to write change to the data store
	k_EResultLockingFailed = 33,				// failed to acquire access lock for this operation
	k_EResultLogonSessionReplaced = 34,
	k_EResultConnectFailed = 35,
	k_EResultHandshakeFailed = 36,
	k_EResultIOFailure = 37,
	k_EResultRemoteDisconnect = 38,
	k_EResultShoppingCartNotFound = 39,			// failed to find the shopping cart requested
	k_EResultBlocked = 40,						// a user didn't allow it
	k_EResultIgnored = 41,						// target is ignoring sender
	k_EResultNoMatch = 42,						// nothing matching the request found
	k_EResultAccountDisabled = 43,
	k_EResultServiceReadOnly = 44,				// this service is not accepting content changes right now
	k_EResultAccountNotFeatured = 45,			// account doesn't have value, so this feature isn't available
	k_EResultAdministratorOK = 46,				// allowed to take this action, but only because requester is admin
	k_EResultContentVersion = 47,				// A Version mismatch in content transmitted within the Steam protocol.
	k_EResultTryAnotherCM = 48,					// The current CM can't service the user making a request, user should try another.
	k_EResultPasswordRequiredToKickSession = 49,// You are already logged in elsewhere, this cached credential login has failed.
	k_EResultAlreadyLoggedInElsewhere = 50,		// You are already logged in elsewhere, you must wait
	k_EResultSuspended = 51,					// Long running operation (content download) suspended/paused
	k_EResultCancelled = 52,					// Operation canceled (typically by user: content download)
	k_EResultDataCorruption = 53,				// Operation canceled because data is ill formed or unrecoverable
	k_EResultDiskFull = 54,						// Operation canceled - not enough disk space.
	k_EResultRemoteCallFailed = 55,				// an remote call or IPC call failed
	k_EResultPasswordUnset = 56,				// Password could not be verified as it's unset server side
	k_EResultExternalAccountUnlinked = 57,		// External account (PSN, Facebook...) is not linked to a Steam account
	k_EResultPSNTicketInvalid = 58,				// PSN ticket was invalid
	k_EResultExternalAccountAlreadyLinked = 59,	// External account (PSN, Facebook...) is already linked to some other account, must explicitly request to replace/delete the link first
	k_EResultRemoteFileConflict = 60,			// The sync cannot resume due to a conflict between the local and remote files
	k_EResultIllegalPassword = 61,				// The requested new password is not legal
	k_EResultSameAsPreviousValue = 62,			// new value is the same as the old one ( secret question and answer )
	k_EResultAccountLogonDenied = 63,			// account login denied due to 2nd factor authentication failure
	k_EResultCannotUseOldPassword = 64,			// The requested new password is not legal
	k_EResultInvalidLoginAuthCode = 65,			// account login denied due to auth code invalid
	k_EResultAccountLogonDeniedNoMail = 66,		// account login denied due to 2nd factor auth failure - and no mail has been sent - partner site specific
	k_EResultHardwareNotCapableOfIPT = 67,		//
	k_EResultIPTInitError = 68,					//
	k_EResultParentalControlRestricted = 69,	// operation failed due to parental control restrictions for current user
	k_EResultFacebookQueryError = 70,			// Facebook query returned an error
	k_EResultExpiredLoginAuthCode = 71,			// account login denied due to auth code expired
	k_EResultIPLoginRestrictionFailed = 72,
	k_EResultAccountLockedDown = 73,
	k_EResultAccountLogonDeniedVerifiedEmailRequired = 74,
	k_EResultNoMatchingURL = 75,
	k_EResultBadResponse = 76,					// parse failure, missing field, etc.
	k_EResultRequirePasswordReEntry = 77,		// The user cannot complete the action until they re-enter their password
	k_EResultValueOutOfRange = 78,				// the value entered is outside the acceptable range
	k_EResultUnexpectedError = 79,				// something happened that we didn't expect to ever happen
	k_EResultDisabled = 80,						// The requested service has been configured to be unavailable
	k_EResultInvalidCEGSubmission = 81,			// The set of files submitted to the CEG server are not valid !
	k_EResultRestrictedDevice = 82,				// The device being used is not allowed to perform this action
	k_EResultRegionLocked = 83,					// The action could not be complete because it is region restricted
	k_EResultRateLimitExceeded = 84,			// Temporary rate limit exceeded, try again later, different from k_EResultLimitExceeded which may be permanent
	k_EResultAccountLoginDeniedNeedTwoFactor = 85,	// Need two-factor code to login
	k_EResultItemDeleted = 86,					// The thing we're trying to access has been deleted
	k_EResultAccountLoginDeniedThrottle = 87,	// login attempt failed, try to throttle response to possible attacker
	k_EResultTwoFactorCodeMismatch = 88,		// two factor code mismatch
	k_EResultTwoFactorActivationCodeMismatch = 89,	// activation code for two-factor didn't match
	k_EResultAccountAssociatedToMultiplePartners = 90,	// account has been associated with multiple partners
	k_EResultNotModified = 91,					// data not modified
	k_EResultNoMobileDevice = 92,				// the account does not have a mobile device associated with it
	k_EResultTimeNotSynced = 93,				// the time presented is out of range or tolerance
	k_EResultSmsCodeFailed = 94,				// SMS code failure (no match, none pending, etc.)
	k_EResultAccountLimitExceeded = 95,			// Too many accounts access this resource
	k_EResultAccountActivityLimitExceeded = 96,	// Too many changes to this account
	k_EResultPhoneActivityLimitExceeded = 97,	// Too many changes to this phone
	k_EResultRefundToWallet = 98,				// Cannot refund to payment method, must use wallet
	k_EResultEmailSendFailure = 99,				// Cannot send an email
	k_EResultNotSettled = 100,					// Can't perform operation till payment has settled
	k_EResultNeedCaptcha = 101,					// Needs to provide a valid captcha
	k_EResultGSLTDenied = 102,					// a game server login token owned by this token's owner has been banned
	k_EResultGSOwnerDenied = 103,				// game server owner is denied for other reason (account lock, community ban, vac ban, missing phone)
	k_EResultInvalidItemType = 104,				// the type of thing we were requested to act on is invalid
	k_EResultIPBanned = 105,					// the ip address has been banned from taking this action
	k_EResultGSLTExpired = 106,					// this token has expired from disuse; can be reset for use
	k_EResultInsufficientFunds = 107,			// user doesn't have enough wallet funds to complete the action
	k_EResultTooManyPending = 108,				// There are too many of this thing pending already
	k_EResultNoSiteLicensesFound = 109,			// No site licenses found
	k_EResultWGNetworkSendExceeded = 110,		// the WG couldn't send a response because we exceeded max network send size
	k_EResultAccountNotFriends = 111,			// the user is not mutually friends
	k_EResultLimitedUserAccount = 112,			// the user is limited
	k_EResultCantRemoveItem = 113,				// item can't be removed
	k_EResultAccountDeleted = 114,				// account has been deleted
	k_EResultExistingUserCancelledLicense = 115,	// A license for this already exists, but cancelled
	k_EResultCommunityCooldown = 116,			// access is denied because of a community cooldown (probably from support profile data resets)
	k_EResultNoLauncherSpecified = 117,			// No launcher was specified, but a launcher was needed to choose correct realm for operation.
	k_EResultMustAgreeToSSA = 118,				// User must agree to china SSA or global SSA before login
	k_EResultLauncherMigrated = 119,			// The specified launcher type is no longer supported; the user should be directed elsewhere
	k_EResultSteamRealmMismatch = 120,			// The user's realm does not match the realm of the requested resource
	k_EResultInvalidSignature = 121,			// signature check did not match
	k_EResultParseFailure = 122,				// Failed to parse input
	k_EResultNoVerifiedPhone = 123,				// account does not have a verified phone number
	k_EResultInsufficientBattery = 124,			// user device doesn't have enough battery charge currently to complete the action
	k_EResultChargerRequired = 125,				// The operation requires a charger to be plugged in, which wasn't present
	k_EResultCachedCredentialInvalid = 126,		// Cached credential was invalid - user must reauthenticate
	K_EResultPhoneNumberIsVOIP = 127,			// The phone number provided is a Voice Over IP number
	k_EResultNotSupported = 128,				// The data being accessed is not supported by this API
	k_EResultFamilySizeLimitExceeded = 129,		// Reached the maximum size of the family
	k_EResultOfflineAppCacheInvalid = 130,		// The local data for the offline mode cache is insufficient to login
	k_EResultTryLater = 131,					// retry the operation later
};
typedef enum EResult EResult;

enum ESteamNetworkingIdentityType
{
	k_ESteamNetworkingIdentityType_Invalid = 0,
	k_ESteamNetworkingIdentityType_SteamID = 16, // 64-bit CSteamID
	k_ESteamNetworkingIdentityType_XboxPairwiseID = 17, // Publisher-specific user identity, as string
	k_ESteamNetworkingIdentityType_SonyPSN = 18, // 64-bit ID
	k_ESteamNetworkingIdentityType_IPAddress = 1,
	k_ESteamNetworkingIdentityType_GenericString = 2,
	k_ESteamNetworkingIdentityType_GenericBytes = 3,
	k_ESteamNetworkingIdentityType_UnknownType = 4,
	k_ESteamNetworkingIdentityType__Force32bit = 0x7fffffff,
};
typedef enum ESteamNetworkingIdentityType ESteamNetworkingIdentityType;

enum ESteamNetworkingConnectionState
{
	k_ESteamNetworkingConnectionState_None = 0,
	k_ESteamNetworkingConnectionState_Connecting = 1,
	k_ESteamNetworkingConnectionState_FindingRoute = 2,
	k_ESteamNetworkingConnectionState_Connected = 3,
	k_ESteamNetworkingConnectionState_ClosedByPeer = 4,
	k_ESteamNetworkingConnectionState_ProblemDetectedLocally = 5,
	k_ESteamNetworkingConnectionState_FinWait = -1,
	k_ESteamNetworkingConnectionState_Linger = -2,
	k_ESteamNetworkingConnectionState_Dead = -3,
	k_ESteamNetworkingConnectionState__Force32Bit = 0x7fffffff
};
typedef enum ESteamNetworkingConnectionState ESteamNetworkingConnectionState;

enum ESteamAPIInitResult
{
	k_ESteamAPIInitResult_OK = 0,
	k_ESteamAPIInitResult_FailedGeneric = 1, // Some other failure
	k_ESteamAPIInitResult_NoSteamClient = 2, // We cannot connect to Steam, steam probably isn't running
	k_ESteamAPIInitResult_VersionMismatch = 3, // Steam client appears to be out of date
};
typedef enum ESteamAPIInitResult ESteamAPIInitResult;

// lobby type description
enum ELobbyType
{
	k_ELobbyTypePrivate = 0,		// only way to join the lobby is to invite to someone else
	k_ELobbyTypeFriendsOnly = 1,	// shows for friends or invitees, but not in public lobby list, allows those who join to invite their own friends
	k_ELobbyTypePublic = 2,			// visible for friends and in lobby list
	k_ELobbyTypeInvisible = 3,		// returned by search, but not visible to other friends
									//    useful if you want a user in two lobbies, for example matching groups together
									//	  a user can be in only one regular lobby, and up to two invisible lobbies
	k_ELobbyTypePrivateUnique = 4,	// private, unique and does not delete when empty - only one of these may exist per unique keypair set
									// can only create from webapi
};
typedef enum ELobbyType ELobbyType;

// -- structs

#pragma pack(push,1)


/// RFC4038, section 4.2
struct IPv4MappedAddress {
// Max length of the buffer needed to hold IP formatted using ToString, including '\0'
// ([0123:4567:89ab:cdef:0123:4567:89ab:cdef]:12345)
// enum { k_cchMaxString = 48 };

	uint64_t m_8zeros;
	uint16_t m_0000;
	uint16_t m_ffff;
	uint8_t m_ip[ 4 ]; // NOTE: As bytes, i.e. network byte order
};
typedef struct IPv4MappedAddress IPv4MappedAddress;

struct SteamNetworkingIPAddr
{
	union
	{
		uint8_t m_ipv6[ 16 ];
		IPv4MappedAddress m_ipv4;
	};
	uint16_t m_port; // Host byte order
};
typedef struct SteamNetworkingIPAddr SteamNetworkingIPAddr;

// Constants for SteamNetworkingIdentity
enum
{
	k_cchMaxString = 128, // Max length of the buffer needed to hold any identity, formatted in string format by ToString
	k_cchMaxGenericString = 32, // Max length of the string for generic string identities.  Including terminating '\0'
	k_cchMaxXboxPairwiseID = 33, // Including terminating '\0'
	k_cbMaxGenericBytes = 32,
	k_cchSteamNetworkingMaxConnectionCloseReason = 128,
	k_cchSteamNetworkingMaxConnectionDescription = 128,
};
struct SteamNetworkingIdentity
{
	ESteamNetworkingIdentityType m_eType;
	int m_cbSize;
	union {
		uint64_t m_steamID64;
		uint64_t m_PSNID;
		char m_szGenericString[ k_cchMaxGenericString ];
		char m_szXboxPairwiseID[ k_cchMaxXboxPairwiseID ];
		uint8_t m_genericBytes[ k_cbMaxGenericBytes ];
		char m_szUnknownRawString[ k_cchMaxString ];
		SteamNetworkingIPAddr m_ip;
		uint32_t m_reserved[ 32 ]; // Pad structure to leave easy room for future expansion
	};
};
typedef struct SteamNetworkingIdentity SteamNetworkingIdentity;
struct SteamNetConnectionRealTimeStatus_t
{
	ESteamNetworkingConnectionState m_eState;
	int m_nPing;
	float m_flConnectionQualityLocal;
	float m_flConnectionQualityRemote;
	float m_flOutPacketsPerSec;
	float m_flOutBytesPerSec;
	float m_flInPacketsPerSec;
	float m_flInBytesPerSec;
	int m_nSendRateBytesPerSecond;
	int m_cbPendingUnreliable;
	int m_cbPendingReliable;
	int m_cbSentUnackedReliable;
	SteamNetworkingMicroseconds m_usecQueueTime;
	int32_t m_usecMaxJitter;
	uint32_t reserved[15];
};
typedef struct SteamNetConnectionRealTimeStatus_t SteamNetConnectionRealTimeStatus_t;

typedef struct SteamNetworkingMessage_t SteamNetworkingMessage_t;
struct SteamNetworkingMessage_t
{
	void *m_pData;
	int m_cbSize;
	HSteamNetConnection m_conn;
	SteamNetworkingIdentity m_identityPeer;
	int64_t m_nConnUserData;
	SteamNetworkingMicroseconds m_usecTimeReceived;
	int64_t m_nMessageNumber;
	void (*m_pfnFreeData)( SteamNetworkingMessage_t *pMsg );
	void (*m_pfnRelease)( SteamNetworkingMessage_t *pMsg );
	int m_nChannel;
	int m_nFlags;
	int64_t m_nUserData;
	uint16_t m_idxLane;
	uint16_t _pad1__;
};

struct SteamNetConnectionInfo_t
{
	SteamNetworkingIdentity m_identityRemote;
	int64_t m_nUserData;
	HSteamListenSocket m_hListenSocket;
	SteamNetworkingIPAddr m_addrRemote;
	uint16_t m__pad1;
	SteamNetworkingPOPID m_idPOPRemote;
	SteamNetworkingPOPID m_idPOPRelay;
	ESteamNetworkingConnectionState m_eState;
	int m_eEndReason;
	char m_szEndDebug[ k_cchSteamNetworkingMaxConnectionCloseReason ];
	char m_szConnectionDescription[ k_cchSteamNetworkingMaxConnectionDescription ];
	int m_nFlags;
	uint32_t reserved[63];
};
typedef struct SteamNetConnectionInfo_t SteamNetConnectionInfo_t;

struct SteamNetworkingMessagesSessionRequest_t
{
	SteamNetworkingIdentity m_identityRemote;			// user who wants to talk to us
};
typedef struct SteamNetworkingMessagesSessionRequest_t SteamNetworkingMessagesSessionRequest_t;

struct SteamNetworkingMessagesSessionFailed_t
{
	SteamNetConnectionInfo_t m_info;
};
typedef struct SteamNetworkingMessagesSessionFailed_t SteamNetworkingMessagesSessionFailed_t;
#pragma pack(pop)


// ISteamNetworkingMessages

typedef struct ISteamNetworkingMessages ISteamNetworkingMessages;
typedef struct ISteamNetworkingUtils ISteamNetworkingUtils;
typedef void (*FnSteamNetworkingMessagesSessionRequest)(SteamNetworkingMessagesSessionRequest_t *);
typedef void (*FnSteamNetworkingMessagesSessionFailed)(SteamNetworkingMessagesSessionFailed_t *);


#if defined( VALVE_CALLBACK_PACK_SMALL )
#pragma pack( push, 4 )
#elif defined( VALVE_CALLBACK_PACK_LARGE )
#pragma pack( push, 8 )
#else
#error steam_api_common.h should define VALVE_CALLBACK_PACK_xxx
#endif

/// Internal structure used in manual callback dispatch
struct CallbackMsg_t
{
	HSteamUser m_hSteamUser; // Specific user to whom this callback applies.
	int m_iCallback; // Callback identifier.  (Corresponds to the k_iCallback enum in the callback structure.)
	uint8_t* m_pubParam; // Points to the callback structure
	int m_cubParam; // Size of the data pointed to by m_pubParam
};
typedef struct CallbackMsg_t CallbackMsg_t;
#pragma pack( pop )

// ISteamMatchmaking
typedef struct ISteamMatchmaking ISteamMatchmaking;

struct LobbyInvite_t
{
	uint64_t m_ulSteamIDUser;		// Steam ID of the person making the invite
	uint64_t m_ulSteamIDLobby;	// Steam ID of the Lobby
	uint64_t m_ulGameID;			// GameID of the Lobby
};
typedef struct LobbyInvite_t LobbyInvite_t;

struct LobbyEnter_t
{
	uint64_t m_ulSteamIDLobby;							// SteamID of the Lobby you have entered
	uint32_t m_rgfChatPermissions;						// Permissions of the current user
	bool m_bLocked;										// If true, then only invited users may join
	uint32_t m_EChatRoomEnterResponse;	// EChatRoomEnterResponse
};
typedef struct LobbyEnter_t LobbyEnter_t;
struct LobbyCreated_t
{
	EResult m_eResult;		// k_EResultOK - the lobby was successfully created
							// k_EResultNoConnection - your Steam client doesn't have a connection to the back-end
							// k_EResultTimeout - you the message to the Steam servers, but it didn't respond
							// k_EResultFail - the server responded, but with an unknown internal error
							// k_EResultAccessDenied - your game isn't set to allow lobbies, or your client does haven't rights to play the game
							// k_EResultLimitExceeded - your game client has created too many lobbies

	uint64_t m_ulSteamIDLobby;		// chat room, zero if failed
};
typedef struct LobbyCreated_t LobbyCreated_t;

// ISteamFriends
typedef struct ISteamFriends ISteamFriends;


// -- SteamAPI
S_API ESteamAPIInitResult S_CALLTYPE SteamAPI_InitFlat(SteamErrMsg* pOutErrMsg);
S_API void S_CALLTYPE SteamAPI_ManualDispatch_Init();
S_API HSteamPipe S_CALLTYPE SteamAPI_GetHSteamPipe();
S_API void S_CALLTYPE SteamAPI_ManualDispatch_RunFrame(HSteamPipe hSteamPipe);
S_API bool S_CALLTYPE SteamAPI_ManualDispatch_GetNextCallback(HSteamPipe hSteamPipe, CallbackMsg_t* pCallbackMsg);
S_API void S_CALLTYPE SteamAPI_ManualDispatch_FreeLastCallback(HSteamPipe hSteamPipe);


// -- SteamUser
// A versioned accessor is exported by the library
S_API ISteamUser* SteamAPI_SteamUser_v023();
// Inline, unversioned accessor to get the current version.  Essentially the same as SteamUser(), but using this ensures that you are using a matching library.
inline ISteamUser* SteamAPI_SteamUser() { return SteamAPI_SteamUser_v023(); }
S_API HSteamUser SteamAPI_ISteamUser_GetHSteamUser(ISteamUser* self);
S_API bool SteamAPI_ISteamUser_BLoggedOn(ISteamUser* self);
S_API uint64_steamid SteamAPI_ISteamUser_GetSteamID(ISteamUser* self);

// ISteamFriends
// A versioned accessor is exported by the library
S_API ISteamFriends *SteamAPI_SteamFriends_v018();
// Inline, unversioned accessor to get the current version.  Essentially the same as SteamFriends(), but using this ensures that you are using a matching library.
inline ISteamFriends *SteamAPI_SteamFriends() { return SteamAPI_SteamFriends_v018(); }
S_API void SteamAPI_ISteamFriends_ActivateGameOverlay( ISteamFriends* self, const char * pchDialog );
S_API void SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog( ISteamFriends* self, uint64_steamid steamIDLobby );

// -- SteamNetworkingMessages
// A versioned accessor is exported by the library
S_API ISteamNetworkingMessages *SteamAPI_SteamNetworkingMessages_SteamAPI_v002();
// Inline, unversioned accessor to get the current version.  Essentially the same as SteamNetworkingMessages_SteamAPI(), but using this ensures that you are using a matching library.
inline ISteamNetworkingMessages *SteamAPI_SteamNetworkingMessages_SteamAPI() { return SteamAPI_SteamNetworkingMessages_SteamAPI_v002(); }
// A versioned accessor is exported by the library
S_API ISteamNetworkingMessages *SteamAPI_SteamGameServerNetworkingMessages_SteamAPI_v002();
// Inline, unversioned accessor to get the current version.  Essentially the same as SteamGameServerNetworkingMessages_SteamAPI(), but using this ensures that you are using a matching library.
inline ISteamNetworkingMessages *SteamAPI_SteamGameServerNetworkingMessages_SteamAPI() { return SteamAPI_SteamGameServerNetworkingMessages_SteamAPI_v002(); }
S_API EResult SteamAPI_ISteamNetworkingMessages_SendMessageToUser( ISteamNetworkingMessages* self, const SteamNetworkingIdentity * identityRemote, const void * pubData, uint32_t cubData, int nSendFlags, int nRemoteChannel );
S_API int SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel( ISteamNetworkingMessages* self, int nLocalChannel, SteamNetworkingMessage_t ** ppOutMessages, int nMaxMessages );
S_API bool SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser( ISteamNetworkingMessages* self, const SteamNetworkingIdentity * identityRemote );
S_API bool SteamAPI_ISteamNetworkingMessages_CloseSessionWithUser( ISteamNetworkingMessages* self, const SteamNetworkingIdentity * identityRemote );
S_API bool SteamAPI_ISteamNetworkingMessages_CloseChannelWithUser( ISteamNetworkingMessages* self, const SteamNetworkingIdentity * identityRemote, int nLocalChannel );
S_API ESteamNetworkingConnectionState SteamAPI_ISteamNetworkingMessages_GetSessionConnectionInfo( ISteamNetworkingMessages* self, const SteamNetworkingIdentity * identityRemote, SteamNetConnectionInfo_t * pConnectionInfo, SteamNetConnectionRealTimeStatus_t * pQuickStatus );
S_API void SteamAPI_SteamNetworkingMessage_t_Release( SteamNetworkingMessage_t* self );

// -- SteamNetworkingUtils
// A versioned accessor is exported by the library
S_API ISteamNetworkingUtils* SteamAPI_SteamNetworkingUtils_SteamAPI_v004();
// Inline, unversioned accessor to get the current version.  Essentially the same as SteamNetworkingUtils_SteamAPI(), but using this ensures that you are using a matching library.
inline ISteamNetworkingUtils* SteamAPI_SteamNetworkingUtils_SteamAPI() { return SteamAPI_SteamNetworkingUtils_SteamAPI_v004(); }
S_API void SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess(ISteamNetworkingUtils* self);

// -- ISteamMatchmaking
// A versioned accessor is exported by the library
S_API ISteamMatchmaking *SteamAPI_SteamMatchmaking_v009();
// Inline, unversioned accessor to get the current version.  Essentially the same as SteamMatchmaking(), but using this ensures that you are using a matching library.
inline ISteamMatchmaking *SteamAPI_SteamMatchmaking() { return SteamAPI_SteamMatchmaking_v009(); }
// S_API int SteamAPI_ISteamMatchmaking_GetFavoriteGameCount( ISteamMatchmaking* self );
// S_API bool SteamAPI_ISteamMatchmaking_GetFavoriteGame( ISteamMatchmaking* self, int iGame, AppId_t * pnAppID, uint32 * pnIP, uint16 * pnConnPort, uint16 * pnQueryPort, uint32 * punFlags, uint32 * pRTime32LastPlayedOnServer );
// S_API int SteamAPI_ISteamMatchmaking_AddFavoriteGame( ISteamMatchmaking* self, AppId_t nAppID, uint32 nIP, uint16 nConnPort, uint16 nQueryPort, uint32 unFlags, uint32 rTime32LastPlayedOnServer );
// S_API bool SteamAPI_ISteamMatchmaking_RemoveFavoriteGame( ISteamMatchmaking* self, AppId_t nAppID, uint32 nIP, uint16 nConnPort, uint16 nQueryPort, uint32 unFlags );
// S_API SteamAPICall_t SteamAPI_ISteamMatchmaking_RequestLobbyList( ISteamMatchmaking* self );
// S_API void SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter( ISteamMatchmaking* self, const char * pchKeyToMatch, const char * pchValueToMatch, ELobbyComparison eComparisonType );
// S_API void SteamAPI_ISteamMatchmaking_AddRequestLobbyListNumericalFilter( ISteamMatchmaking* self, const char * pchKeyToMatch, int nValueToMatch, ELobbyComparison eComparisonType );
// S_API void SteamAPI_ISteamMatchmaking_AddRequestLobbyListNearValueFilter( ISteamMatchmaking* self, const char * pchKeyToMatch, int nValueToBeCloseTo );
// S_API void SteamAPI_ISteamMatchmaking_AddRequestLobbyListFilterSlotsAvailable( ISteamMatchmaking* self, int nSlotsAvailable );
// S_API void SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter( ISteamMatchmaking* self, ELobbyDistanceFilter eLobbyDistanceFilter );
// S_API void SteamAPI_ISteamMatchmaking_AddRequestLobbyListResultCountFilter( ISteamMatchmaking* self, int cMaxResults );
// S_API void SteamAPI_ISteamMatchmaking_AddRequestLobbyListCompatibleMembersFilter( ISteamMatchmaking* self, uint64_steamid steamIDLobby );
// S_API uint64_steamid SteamAPI_ISteamMatchmaking_GetLobbyByIndex( ISteamMatchmaking* self, int iLobby );
S_API SteamAPICall_t SteamAPI_ISteamMatchmaking_CreateLobby( ISteamMatchmaking* self, ELobbyType eLobbyType, int cMaxMembers );
S_API SteamAPICall_t SteamAPI_ISteamMatchmaking_JoinLobby( ISteamMatchmaking* self, uint64_steamid steamIDLobby );
S_API void SteamAPI_ISteamMatchmaking_LeaveLobby( ISteamMatchmaking* self, uint64_steamid steamIDLobby );
// S_API bool SteamAPI_ISteamMatchmaking_InviteUserToLobby( ISteamMatchmaking* self, uint64_steamid steamIDLobby, uint64_steamid steamIDInvitee );
S_API int SteamAPI_ISteamMatchmaking_GetNumLobbyMembers( ISteamMatchmaking* self, uint64_steamid steamIDLobby );
S_API uint64_steamid SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex( ISteamMatchmaking* self, uint64_steamid steamIDLobby, int iMember );
// S_API const char * SteamAPI_ISteamMatchmaking_GetLobbyData( ISteamMatchmaking* self, uint64_steamid steamIDLobby, const char * pchKey );
// S_API bool SteamAPI_ISteamMatchmaking_SetLobbyData( ISteamMatchmaking* self, uint64_steamid steamIDLobby, const char * pchKey, const char * pchValue );
// S_API int SteamAPI_ISteamMatchmaking_GetLobbyDataCount( ISteamMatchmaking* self, uint64_steamid steamIDLobby );
// S_API bool SteamAPI_ISteamMatchmaking_GetLobbyDataByIndex( ISteamMatchmaking* self, uint64_steamid steamIDLobby, int iLobbyData, char * pchKey, int cchKeyBufferSize, char * pchValue, int cchValueBufferSize );
// S_API bool SteamAPI_ISteamMatchmaking_DeleteLobbyData( ISteamMatchmaking* self, uint64_steamid steamIDLobby, const char * pchKey );
// S_API const char * SteamAPI_ISteamMatchmaking_GetLobbyMemberData( ISteamMatchmaking* self, uint64_steamid steamIDLobby, uint64_steamid steamIDUser, const char * pchKey );
// S_API void SteamAPI_ISteamMatchmaking_SetLobbyMemberData( ISteamMatchmaking* self, uint64_steamid steamIDLobby, const char * pchKey, const char * pchValue );
// S_API bool SteamAPI_ISteamMatchmaking_SendLobbyChatMsg( ISteamMatchmaking* self, uint64_steamid steamIDLobby, const void * pvMsgBody, int cubMsgBody );
// S_API int SteamAPI_ISteamMatchmaking_GetLobbyChatEntry( ISteamMatchmaking* self, uint64_steamid steamIDLobby, int iChatID, CSteamID * pSteamIDUser, void * pvData, int cubData, EChatEntryType * peChatEntryType );
// S_API bool SteamAPI_ISteamMatchmaking_RequestLobbyData( ISteamMatchmaking* self, uint64_steamid steamIDLobby );
// S_API void SteamAPI_ISteamMatchmaking_SetLobbyGameServer( ISteamMatchmaking* self, uint64_steamid steamIDLobby, uint32 unGameServerIP, uint16 unGameServerPort, uint64_steamid steamIDGameServer );
// S_API bool SteamAPI_ISteamMatchmaking_GetLobbyGameServer( ISteamMatchmaking* self, uint64_steamid steamIDLobby, uint32 * punGameServerIP, uint16 * punGameServerPort, CSteamID * psteamIDGameServer );
// S_API bool SteamAPI_ISteamMatchmaking_SetLobbyMemberLimit( ISteamMatchmaking* self, uint64_steamid steamIDLobby, int cMaxMembers );
// S_API int SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit( ISteamMatchmaking* self, uint64_steamid steamIDLobby );
// S_API bool SteamAPI_ISteamMatchmaking_SetLobbyType( ISteamMatchmaking* self, uint64_steamid steamIDLobby, ELobbyType eLobbyType );
// S_API bool SteamAPI_ISteamMatchmaking_SetLobbyJoinable( ISteamMatchmaking* self, uint64_steamid steamIDLobby, bool bLobbyJoinable );
// S_API uint64_steamid SteamAPI_ISteamMatchmaking_GetLobbyOwner( ISteamMatchmaking* self, uint64_steamid steamIDLobby );
// S_API bool SteamAPI_ISteamMatchmaking_SetLobbyOwner( ISteamMatchmaking* self, uint64_steamid steamIDLobby, uint64_steamid steamIDNewOwner );
// S_API bool SteamAPI_ISteamMatchmaking_SetLinkedLobby( ISteamMatchmaking* self, uint64_steamid steamIDLobby, uint64_steamid steamIDLobbyDependent );

#endif
