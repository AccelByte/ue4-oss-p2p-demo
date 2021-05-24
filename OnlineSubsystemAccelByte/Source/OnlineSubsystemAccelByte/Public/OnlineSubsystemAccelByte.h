// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#pragma once

#include "CoreMinimal.h"
#include "CoreMinimal.h"
#include "OnlineSubsystemImpl.h"
#include "OnlineSubsystemAccelBytePackage.h"
#include "OnlineSubsystemAccelByteConstant.h"

class FOnlineIdentityAccelByte;
class FOnlineSessionAccelByte;
class FOnlineIdentityAccelByte;
class FOnlineExternalUIAccelByte;

typedef TSharedPtr<class FOnlineSessionAccelByte, ESPMode::ThreadSafe> FOnlineSessionAccelBytePtr;
typedef TSharedPtr<class FOnlineIdentityAccelByte, ESPMode::ThreadSafe> FOnlineIdentityAccelBytePtr;
typedef TSharedPtr<class FOnlineExternalUIAccelByte, ESPMode::ThreadSafe> FOnlineExternalUIAccelBytePtr;

class ONLINESUBSYSTEMACCELBYTE_API FOnlineSubsystemAccelByte : public FOnlineSubsystemImpl
{
public:
	virtual ~FOnlineSubsystemAccelByte();

	//~ Begin IOnlineSubsystem Interface
	virtual bool Init() override;
	virtual FString GetAppId() const override;
	virtual FText GetOnlineServiceName() const override;
	virtual IOnlineSessionPtr GetSessionInterface() const override;
	virtual IOnlineFriendsPtr GetFriendsInterface() const override;
	virtual IOnlineIdentityPtr GetIdentityInterface() const override;
	virtual IOnlineExternalUIPtr GetExternalUIInterface() const override;
	//~ End IOnlineSubsystem Interface

	// FTickerObjectBase
	virtual bool Tick(float DeltaTime) override;

PACKAGE_SCOPE:
	/** Only the factory makes instances */
	FOnlineSubsystemAccelByte() = delete;
	explicit FOnlineSubsystemAccelByte(FName InInstanceName) :
		FOnlineSubsystemImpl(ACCELBYTE_SUBSYSTEM, InInstanceName),
		SessionInterface(nullptr),
		IdentityInterface(nullptr),
		ExternalUIInterface(nullptr)
	{}

private:
	bool bAutoLoginDevice = false;
	bool bLoginInProcess = false;
	bool bLoggedIn = false;
	FOnlineSessionAccelBytePtr SessionInterface;
	FOnlineIdentityAccelBytePtr IdentityInterface;
	FOnlineExternalUIAccelBytePtr ExternalUIInterface;

	void OnLoginCallback(int num, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);
};

typedef TSharedPtr<FOnlineSubsystemAccelByte, ESPMode::ThreadSafe> FOnlineSubsystemAccelBytePtr;