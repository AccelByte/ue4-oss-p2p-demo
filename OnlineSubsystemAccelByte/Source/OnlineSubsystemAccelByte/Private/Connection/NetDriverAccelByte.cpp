// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#include "NetDriverAccelByte.h"
#include "OnlineSubsystemAccelByteConstant.h"
#include "SocketAccelByte.h"
#include "OnlineSubsystemAccelByte.h"

void UIpNetDriverAccelByte::PostInitProperties() 
{
	Super::PostInitProperties();
}

ISocketSubsystem* UIpNetDriverAccelByte::GetSocketSubsystem() 
{
	return ISocketSubsystem::Get(bIsForward ? PLATFORM_SOCKETSUBSYSTEM : ACCELBYTE_SUBSYSTEM);
}

bool UIpNetDriverAccelByte::IsAvailable() const
{
	IOnlineSubsystem* AccelByteSubsystem = IOnlineSubsystem::Get(ACCELBYTE_SUBSYSTEM);
	if (AccelByteSubsystem)
	{
		ISocketSubsystem* Sockets = ISocketSubsystem::Get(ACCELBYTE_SUBSYSTEM);
		if (Sockets)
		{
			return true;
		}
	}

	return false;
}

bool UIpNetDriverAccelByte::InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error)
{
	if (bIsForward)
	{
		return UIpNetDriver::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error);
	}

	if (!UNetDriver::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error))
	{
		return false;
	}

	ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();
	if (SocketSubsystem == nullptr)
	{
		UE_LOG(LogNet, Warning, TEXT("Unable to find socket subsystem"));
		Error = TEXT("Unable to find socket subsystem");
		return false;
	}

	if (GetSocket() == nullptr)
	{
		Error = FString::Printf(TEXT("Error get socket AccelByte"));
		return false;
	}

	LocalAddr = SocketSubsystem->GetLocalBindAddr(*GLog);

	LocalAddr->SetPort(URL.Port);

	const int32 BoundPort = SocketSubsystem->BindNextPort(GetSocket(), *LocalAddr, MaxPortCountToTry + 1, 1);
	UE_LOG(LogNet, Display, TEXT("%s bound to port %d"), *GetName(), BoundPort);

	return true;
}

bool UIpNetDriverAccelByte::InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error)
{
	ISocketSubsystem* SocketAccelByte = ISocketSubsystem::Get(ACCELBYTE_SUBSYSTEM);
	if (SocketAccelByte)
	{
		if (ConnectURL.Host.StartsWith(ACCELBYTE_URL_PREFIX))
		{
			SetSocketAndLocalAddress(SocketAccelByte->CreateSocket(FName(TEXT("ClientSocketAccelByte")), TEXT("Unreal client (AccelByte)"), ACCELBYTE_SUBSYSTEM));
		}
		else
		{
			bIsForward = true;
		}
	}
	return Super::InitConnect(InNotify, ConnectURL, Error);
}

bool UIpNetDriverAccelByte::InitListen(FNetworkNotify* InNotify, FURL& ListenURL, bool bReuseAddressAndPort, FString& Error)
{
	ISocketSubsystem* SocketAccelByte = ISocketSubsystem::Get(ACCELBYTE_SUBSYSTEM);
	if (SocketAccelByte && !ListenURL.HasOption(TEXT("bIsLanMatch")) && !IsRunningDedicatedServer())
	{
		const FName SocketTypeName = FName(TEXT("ServerSocketAccelByte"));
		SetSocketAndLocalAddress(SocketAccelByte->CreateSocket(SocketTypeName, TEXT("Unreal server (AccelByte)"), ACCELBYTE_SUBSYSTEM));
	}
	else
	{
		bIsForward = true;
	}

	return Super::InitListen(InNotify, ListenURL, bReuseAddressAndPort, Error);
}

void UIpNetDriverAccelByte::Shutdown()
{
	Super::Shutdown();
}

bool UIpNetDriverAccelByte::IsNetResourceValid()
{
	const bool ValidSocket = !bIsForward && (GetSocket() != nullptr);
	const bool ValidFwSocket = bIsForward && UIpNetDriver::IsNetResourceValid();
	return ValidSocket || ValidFwSocket;
}