// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#pragma once

#define ACCELBYTE_SUBSYSTEM FName(TEXT("ACCELBYTE"))

#define ACCELBYTE_URL_PREFIX TEXT("accelbyte.")

#define SOCKET_ACCELBYTE_NAME TEXT("SocketAccelByte")

#define ACCELBYTE_SOCKET_PORT 11223;

UENUM(BluePrintType)
enum class EAccelByteLoginType : uint8
{
	DeviceId,
	UsernamePassword
};