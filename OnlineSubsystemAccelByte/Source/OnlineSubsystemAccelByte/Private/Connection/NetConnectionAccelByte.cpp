// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#include "NetConnectionAccelByte.h"
#include "OnlineSubsystemAccelByteTypes.h"
#include "OnlineSubsystemAccelByteConstant.h"
#include "SocketAccelByte.h"
#include "NetDriverAccelByte.h"

void UIpConnectionAccelByte::InitRemoteConnection(class UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL,
	const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	bIsForward = static_cast<UIpNetDriverAccelByte*>(InDriver)->bIsForward;
	if (!bIsForward)
	{
		DisableAddressResolution();
	}
	Super::InitRemoteConnection(InDriver, InSocket, InURL, InRemoteAddr, InState, InMaxPacket, InPacketOverhead);
}

void UIpConnectionAccelByte::InitLocalConnection(class UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL,
	EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	bIsForward = InURL.Host.StartsWith(ACCELBYTE_URL_PREFIX) ? false : true;
	if (!bIsForward)
	{
		DisableAddressResolution();
	}
	Super::InitLocalConnection(InDriver, InSocket, InURL, InState, InMaxPacket, InPacketOverhead);
}

void UIpConnectionAccelByte::CleanUp()
{
	Super::CleanUp();
}
