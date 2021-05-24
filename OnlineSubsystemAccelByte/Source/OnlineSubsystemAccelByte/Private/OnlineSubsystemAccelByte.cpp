// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#include "OnlineSubsystemAccelByte.h"
#include "OnlineSessionAccelByte.h"
#include "OnlineIdentityAccelByte.h"
#include "OnlineExternalUIAccelByte.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineSubsystemAccelByteLog.h"
#include "Networking/AccelByteNetworkManager.h"
#include "SocketSubsystemModule.h"
#include "Connection/SocketSubsystemAccelByte.h"
#include "Networking/AccelByteWebSocket.h"

#define LOCTEXT_NAMESPACE "FOnlineSubsystemAccelByte"

bool FOnlineSubsystemAccelByte::Init()
{
	SessionInterface = MakeShareable(new FOnlineSessionAccelByte(this));
	IdentityInterface = MakeShareable(new FOnlineIdentityAccelByte(this));
	ExternalUIInterface = MakeShareable(new FOnlineExternalUIAccelByte(this));
	GConfig->GetBool(TEXT("OnlineSubsystemAccelByte"), TEXT("bAutoLoginUsingDeviceId"), bAutoLoginDevice, GEngineIni);

	IdentityInterface->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateRaw(this, &FOnlineSubsystemAccelByte::OnLoginCallback));

	FSocketSubsystemAccelByte* SocketSubsystem = FSocketSubsystemAccelByte::Create();
	FString Error;
	if (SocketSubsystem->Init(Error))
	{
		FSocketSubsystemModule& SSS = FModuleManager::LoadModuleChecked<FSocketSubsystemModule>("Sockets");
		SSS.RegisterSocketSubsystem(ACCELBYTE_SUBSYSTEM, SocketSubsystem, true);
		UE_LOG_AB(Log, TEXT("AccelByte socket subsystem registered"));

#if USE_SIMPLE_SIGNALING
		AccelByteWebSocket::Instance()->Init();
#endif
	}

	return true;
}

FOnlineSubsystemAccelByte::~FOnlineSubsystemAccelByte()
{
}

FString FOnlineSubsystemAccelByte::GetAppId() const
{
	return TEXT("");
}

FText FOnlineSubsystemAccelByte::GetOnlineServiceName() const
{
	return NSLOCTEXT("OnlineSubsystemAccelByte", "OnlineServiceName", "AccelByte");
}

IOnlineSessionPtr FOnlineSubsystemAccelByte::GetSessionInterface() const
{
	return SessionInterface;
}

IOnlineFriendsPtr FOnlineSubsystemAccelByte::GetFriendsInterface() const
{
	return nullptr;
}

IOnlineIdentityPtr FOnlineSubsystemAccelByte::GetIdentityInterface() const
{
	return IdentityInterface;
}

IOnlineExternalUIPtr FOnlineSubsystemAccelByte::GetExternalUIInterface() const
{
	return ExternalUIInterface;
}

bool FOnlineSubsystemAccelByte::Tick(float DeltaTime) 
{
	if (!FOnlineSubsystemImpl::Tick(DeltaTime))
	{
		return false;
	}
	if (SessionInterface.IsValid())
	{
		SessionInterface->Tick(DeltaTime);
	}
	//Only support 1 login
#if USE_SIMPLE_SIGNALING
	if (!bLoginInProcess && IdentityInterface->GetLoginStatus(0) == ELoginStatus::Type::NotLoggedIn) 
	{
		bLoginInProcess = true;
		IdentityInterface->Login(0, FOnlineAccountCredentials("DeviceId", "", ""));		
	}
#endif
	//AccelByteNetworkManager::Instance().Tick(DeltaTime); 
	return true;
}

void FOnlineSubsystemAccelByte::OnLoginCallback(int num, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	bLoggedIn = bWasSuccessful;
	if (bWasSuccessful) {
		AccelByteNetworkManager::Instance().Run();
	}
}

#undef LOCTEXT_NAMESPACE