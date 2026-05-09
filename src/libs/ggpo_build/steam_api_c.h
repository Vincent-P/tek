#ifndef STEAMAPIC_H
#define STEAMAPIC_H

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

enum { k_cchMaxSteamErrMsg = 1024 };
typedef char SteamErrMsg[ k_cchMaxSteamErrMsg ];

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


#pragma pack(push,1)
// Max length of the buffer needed to hold IP formatted using ToString, including '\0'
// ([0123:4567:89ab:cdef:0123:4567:89ab:cdef]:12345)
// enum { k_cchMaxString = 48 };

/// RFC4038, section 4.2
struct IPv4MappedAddress {
	uint64 m_8zeros;
	uint16 m_0000;
	uint16 m_ffff;
	uint8 m_ip[ 4 ]; // NOTE: As bytes, i.e. network byte order
};
typedef struct IPv4MappedAddress IPv4MappedAddress;

struct SteamNetworkingIPAddr
{
	union
	{
		uint8 m_ipv6[ 16 ];
		IPv4MappedAddress m_ipv4;
	};
	uint16 m_port; // Host byte order
};
typedef struct SteamNetworkingIPAddr SteamNetworkingIPAddr;

enum {
	k_cchMaxString = 128, // Max length of the buffer needed to hold any identity, formatted in string format by ToString
	k_cchMaxGenericString = 32, // Max length of the string for generic string identities.  Including terminating '\0'
	k_cchMaxXboxPairwiseID = 33, // Including terminating '\0'
	k_cbMaxGenericBytes = 32,
	k_cchSteamNetworkingMaxConnectionCloseReason = 128,
	k_cchSteamNetworkingMaxConnectionDescription = 128,
};

typedef uint32 HSteamNetConnection;
typedef uint32 HSteamListenSocket;
typedef int64 SteamNetworkingMicroseconds;
typedef uint32 SteamNetworkingPOPID;

struct SteamNetworkingIdentity
{
	ESteamNetworkingIdentityType m_eType;
	int m_cbSize;
	union {
		uint64 m_steamID64;
		uint64 m_PSNID;
		char m_szGenericString[ k_cchMaxGenericString ];
		char m_szXboxPairwiseID[ k_cchMaxXboxPairwiseID ];
		uint8 m_genericBytes[ k_cbMaxGenericBytes ];
		char m_szUnknownRawString[ k_cchMaxString ];
		SteamNetworkingIPAddr m_ip;
		uint32 m_reserved[ 32 ]; // Pad structure to leave easy room for future expansion
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
	int32 m_usecMaxJitter;
	uint32 reserved[15];
};
typedef struct SteamNetConnectionRealTimeStatus_t SteamNetConnectionRealTimeStatus_t;

typedef struct SteamNetworkingMessage_t SteamNetworkingMessage_t;
struct SteamNetworkingMessage_t
{
	void *m_pData;
	int m_cbSize;
	HSteamNetConnection m_conn;
	SteamNetworkingIdentity m_identityPeer;
	int64 m_nConnUserData;
	SteamNetworkingMicroseconds m_usecTimeReceived;
	int64 m_nMessageNumber;
	void (*m_pfnFreeData)( SteamNetworkingMessage_t *pMsg );
	void (*m_pfnRelease)( SteamNetworkingMessage_t *pMsg );
	int m_nChannel;
	int m_nFlags;
	int64 m_nUserData;
	uint16 m_idxLane;
	uint16 _pad1__;
};

struct SteamNetConnectionInfo_t
{
	SteamNetworkingIdentity m_identityRemote;
	int64 m_nUserData;
	HSteamListenSocket m_hListenSocket;
	SteamNetworkingIPAddr m_addrRemote;
	uint16 m__pad1;
	SteamNetworkingPOPID m_idPOPRemote;
	SteamNetworkingPOPID m_idPOPRelay;
	ESteamNetworkingConnectionState m_eState;
	int m_eEndReason;
	char m_szEndDebug[ k_cchSteamNetworkingMaxConnectionCloseReason ];
	char m_szConnectionDescription[ k_cchSteamNetworkingMaxConnectionDescription ];
	int m_nFlags;
	uint32 reserved[63];
};
typedef struct SteamNetConnectionInfo_t SteamNetConnectionInfo_t;


enum {
	k_iSteamNetworkingMessagesCallbacks = 1250,
	k_iSteamNetworkingMessagesSessionRequestCallback = k_iSteamNetworkingMessagesCallbacks + 1,
	k_iSteamNetworkingSteamNetworkingMessagesSessionFailedCallback = k_iSteamNetworkingMessagesCallbacks + 2,
};

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


#define S_API __declspec( dllimport )
#define S_CALLTYPE __cdecl

// ISteamNetworkingMessages

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

typedef struct ISteamNetworkingMessages ISteamNetworkingMessages;
typedef struct ISteamNetworkingUtils ISteamNetworkingUtils;
typedef void (*FnSteamNetworkingMessagesSessionRequest)(SteamNetworkingMessagesSessionRequest_t *);
typedef void (*FnSteamNetworkingMessagesSessionFailed)(SteamNetworkingMessagesSessionFailed_t *);

// A versioned accessor is exported by the library
S_API ISteamNetworkingMessages *SteamAPI_SteamNetworkingMessages_SteamAPI_v002();
// Inline, unversioned accessor to get the current version.  Essentially the same as SteamNetworkingMessages_SteamAPI(), but using this ensures that you are using a matching library.
inline ISteamNetworkingMessages *SteamAPI_SteamNetworkingMessages_SteamAPI() { return SteamAPI_SteamNetworkingMessages_SteamAPI_v002(); }

// A versioned accessor is exported by the library
S_API ISteamNetworkingMessages *SteamAPI_SteamGameServerNetworkingMessages_SteamAPI_v002();
// Inline, unversioned accessor to get the current version.  Essentially the same as SteamGameServerNetworkingMessages_SteamAPI(), but using this ensures that you are using a matching library.
inline ISteamNetworkingMessages *SteamAPI_SteamGameServerNetworkingMessages_SteamAPI() { return SteamAPI_SteamGameServerNetworkingMessages_SteamAPI_v002(); }
S_API EResult SteamAPI_ISteamNetworkingMessages_SendMessageToUser( ISteamNetworkingMessages* self, const SteamNetworkingIdentity * identityRemote, const void * pubData, uint32 cubData, int nSendFlags, int nRemoteChannel );
S_API int SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel( ISteamNetworkingMessages* self, int nLocalChannel, SteamNetworkingMessage_t ** ppOutMessages, int nMaxMessages );
S_API bool SteamAPI_ISteamNetworkingMessages_AcceptSessionWithUser( ISteamNetworkingMessages* self, const SteamNetworkingIdentity * identityRemote );
S_API bool SteamAPI_ISteamNetworkingMessages_CloseSessionWithUser( ISteamNetworkingMessages* self, const SteamNetworkingIdentity * identityRemote );
S_API bool SteamAPI_ISteamNetworkingMessages_CloseChannelWithUser( ISteamNetworkingMessages* self, const SteamNetworkingIdentity * identityRemote, int nLocalChannel );
S_API ESteamNetworkingConnectionState SteamAPI_ISteamNetworkingMessages_GetSessionConnectionInfo( ISteamNetworkingMessages* self, const SteamNetworkingIdentity * identityRemote, SteamNetConnectionInfo_t * pConnectionInfo, SteamNetConnectionRealTimeStatus_t * pQuickStatus );
S_API void SteamAPI_SteamNetworkingMessage_t_Release( SteamNetworkingMessage_t* self );

S_API ISteamNetworkingUtils *SteamAPI_SteamNetworkingUtils_SteamAPI_v004();
S_API bool SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_MessagesSessionRequest( ISteamNetworkingUtils* self, FnSteamNetworkingMessagesSessionRequest fnCallback );
S_API bool SteamAPI_ISteamNetworkingUtils_SetGlobalCallback_MessagesSessionFailed( ISteamNetworkingUtils* self, FnSteamNetworkingMessagesSessionFailed fnCallback );

#endif
