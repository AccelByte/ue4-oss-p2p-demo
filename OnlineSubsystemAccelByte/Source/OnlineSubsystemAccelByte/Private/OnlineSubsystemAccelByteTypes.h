// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemTypes.h"
#include "IPAddress.h"
#include "OnlineSubsystemAccelBytePackage.h"
#include "OnlineSubsystemAccelByteConstant.h"

class FOnlineSubsystemAccelByte;

//Use the generated one for now
TEMP_UNIQUENETIDSTRING_SUBCLASS(FUniqueNetIdAccelByte, ACCELBYTE_SUBSYSTEM);

class FOnlineSessionInfoAccelByte : public FOnlineSessionInfo
{
protected:
	FOnlineSessionInfoAccelByte(const FOnlineSessionInfoAccelByte& Src)
	{
	}

	FOnlineSessionInfoAccelByte& operator=(const FOnlineSessionInfoAccelByte& Src)
	{
		return *this;
	}

PACKAGE_SCOPE:

	FOnlineSessionInfoAccelByte();

	void Init(const FOnlineSubsystemAccelByte& Subsystem);

	/** The ip & port that the host is listening on (valid for LAN/GameServer) */
	TSharedPtr<class FInternetAddr> HostAddr;
	/** Remote ID of the P2P connection */
	FString RemoteId;
	/** Unique Id for this session */
	FUniqueNetIdAccelByte SessionId;

public:

	virtual ~FOnlineSessionInfoAccelByte() {}

	bool operator==(const FOnlineSessionInfoAccelByte& Other) const
	{
		return false;
	}

	virtual const uint8* GetBytes() const override
	{
		return nullptr;
	}

	virtual int32 GetSize() const override
	{
		return sizeof(uint64) + sizeof(TSharedPtr<class FInternetAddr>);
	}

	virtual bool IsValid() const override
	{
		return (HostAddr.IsValid() && HostAddr->IsValid()) || !RemoteId.IsEmpty();
	}

	virtual FString ToString() const override
	{
		return SessionId.ToString();
	}

	virtual FString ToDebugString() const override
	{
		if(!RemoteId.IsEmpty())
		{
			return FString::Printf(TEXT("ID: %s SessionId: %s"),
                                   *RemoteId,
                                   *SessionId.ToDebugString());	
		}
		return FString::Printf(TEXT("HOST: %s SessionId: %s"),
                                   *RemoteId,
                                   *SessionId.ToDebugString());
	}

	virtual const FUniqueNetId& GetSessionId() const override
	{
		return SessionId;
	}
};
