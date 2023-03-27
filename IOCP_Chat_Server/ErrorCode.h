#pragma once

enum class ERROR_CODE : unsigned short
{
	NONE = 0,

	USER_MGR_INVALID_USER_INDEX = 11,
	USER_MGR_INVALID_USER_UNIQUEID = 12,

	LOGIN_USER_ALREADY = 31,
	LOGIN_USER_USED_ALL_OBJ = 32,
	LOGIN_USER_INVALID_PW = 33,
	LOGIN_USER_NOT_FIND = 34,

	NEW_ROOM_USED_ALL_OBJ = 41,
	NEW_ROOM_FAIL_ENTER = 42,

	ROOM_INVALID_INDEX = 51,
	ROOM_NOT_FIND_USER = 52,
	ROOM_INVALID_USER_STATUS = 53,
	ROOM_USER_NOT_IN = 54,

	ENTER_ROOM_FULL_USER = 61,
	ENTER_ROOM_ALREADY = 62,
};