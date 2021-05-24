// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemAccelByteTypes.h"
#include "NboSerializer.h"

class FNboSerializeToBufferAccelByte : public FNboSerializeToBuffer
{
public:
	FNboSerializeToBufferAccelByte() : FNboSerializeToBuffer(512) 
	{
	}
	
	FNboSerializeToBufferAccelByte(uint32 Size) : FNboSerializeToBuffer(Size) 
	{
	}

	friend inline FNboSerializeToBufferAccelByte& operator<<(FNboSerializeToBufferAccelByte& Ar, const FOnlineSessionInfoAccelByte& SessionInfo)
	{
		check(SessionInfo.HostAddr.IsValid());
		Ar << SessionInfo.SessionId;
		Ar << *SessionInfo.HostAddr;
		return Ar;
	}

	friend inline FNboSerializeToBufferAccelByte& operator<<(FNboSerializeToBufferAccelByte& Ar, const FUniqueNetIdAccelByte& UniqueId)
	{
		Ar << UniqueId.UniqueNetIdStr;
		return Ar;
	}
};

class FNboSerializeFromBufferAccelByte : public FNboSerializeFromBuffer
{
public:
	FNboSerializeFromBufferAccelByte(uint8* Packet,int32 Length) : FNboSerializeFromBuffer(Packet,Length) 
	{
	}
	
	friend inline FNboSerializeFromBufferAccelByte& operator>>(FNboSerializeFromBufferAccelByte& Ar, FOnlineSessionInfoAccelByte& SessionInfo)
	{
		check(SessionInfo.HostAddr.IsValid());
		Ar >> SessionInfo.SessionId; 
		Ar >> *SessionInfo.HostAddr;
		return Ar;
	}
	
	friend inline FNboSerializeFromBufferAccelByte& operator>>(FNboSerializeFromBufferAccelByte& Ar, FUniqueNetIdAccelByte& UniqueId)
	{
		Ar >> UniqueId.UniqueNetIdStr;
		return Ar;
	}
};

