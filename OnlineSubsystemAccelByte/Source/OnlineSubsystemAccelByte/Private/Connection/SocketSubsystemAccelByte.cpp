// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#include "SocketSubsystemAccelByte.h"
#include "IpAddressAccelByte.h"
#include "SocketAccelByte.h"

static FSocketSubsystemAccelByte* SocketSubsystemAccelByteInstance = nullptr;

FSocketSubsystemAccelByte* FSocketSubsystemAccelByte::Create()
{
	if (SocketSubsystemAccelByteInstance == nullptr) 
	{
		SocketSubsystemAccelByteInstance = new FSocketSubsystemAccelByte();
	}
	return SocketSubsystemAccelByteInstance;
}

bool FSocketSubsystemAccelByte::Init(FString& Error)
{
	return true;
}

void FSocketSubsystemAccelByte::Shutdown()
{
	//TODO: clean WebRTC
}

FSocket* FSocketSubsystemAccelByte::CreateSocket(const FName& SocketType, const FString& SocketDescription, const FName& ProtocolName)
{
	FSocket* NewSocket = nullptr;
	if (SocketType == FName("ClientSocketAccelByte"))
	{
		NewSocket = new FSocketAccelByte(false);
	}
	else if (SocketType == FName("ServerSocketAccelByte"))
	{
		NewSocket = new FSocketAccelByte(true);
	}
	else
	{
		ISocketSubsystem* PlatformSocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (PlatformSocketSub)
		{
			NewSocket = PlatformSocketSub->CreateSocket(SocketType, SocketDescription, ProtocolName);
		}
	}

	if (!NewSocket)
	{
		UE_LOG(LogSockets, Warning, TEXT("Failed to create socket %s [%s]"), *SocketType.ToString(), *SocketDescription);
	}

	return NewSocket;
}

void FSocketSubsystemAccelByte::DestroySocket(FSocket* Socket)
{

}

FAddressInfoResult FSocketSubsystemAccelByte::GetAddressInfo(const TCHAR* HostName, const TCHAR* ServiceName,
	EAddressInfoFlags QueryFlags, const FName ProtocolTypeName, ESocketType SocketType)
{
	FString RawAddress(HostName);

	RawAddress.RemoveFromStart(ACCELBYTE_URL_PREFIX);

	if (HostName != nullptr && !RawAddress.IsEmpty())
	{
		FAddressInfoResult Result(HostName, ServiceName);

		FString PortString(ServiceName);
		Result.ReturnCode = SE_NO_ERROR;
		TSharedRef<FInternetAddrAccelByte> IpAddr = StaticCastSharedRef<FInternetAddrAccelByte>(CreateInternetAddr());
		IpAddr->NetId = FUniqueNetIdAccelByte(RawAddress);
		Result.Results.Add(FAddressInfoResultData(IpAddr, 0, ACCELBYTE_SUBSYSTEM, SOCKTYPE_Unknown));
		return Result;
	}

	return ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetAddressInfo(HostName, ServiceName, QueryFlags, ProtocolTypeName, SocketType);
}

TSharedPtr<FInternetAddr> FSocketSubsystemAccelByte::GetAddressFromString(const FString& InAddress)
{
	TSharedRef<FInternetAddrAccelByte> IpAddr = StaticCastSharedRef<FInternetAddrAccelByte>(CreateInternetAddr());
	IpAddr->NetId = FUniqueNetIdAccelByte(InAddress);
	return IpAddr;
}

bool FSocketSubsystemAccelByte::RequiresChatDataBeSeparate()
{
	return false;
}

bool FSocketSubsystemAccelByte::RequiresEncryptedPackets()
{
	//WebRTC connection already encrypted
	return false;
}

bool FSocketSubsystemAccelByte::GetHostName(FString& HostName)
{
	return false;
}

bool FSocketSubsystemAccelByte::HasNetworkDevice()
{
	return false;
}

const TCHAR* FSocketSubsystemAccelByte::GetSocketAPIName() const
{
	return SOCKET_ACCELBYTE_NAME;
}

ESocketErrors FSocketSubsystemAccelByte::GetLastErrorCode()
{
	//TODO: check the webRTC error connection
	return ESocketErrors::SE_NO_ERROR;
}

ESocketErrors FSocketSubsystemAccelByte::TranslateErrorCode(int32 Code)
{
	return ESocketErrors::SE_NO_ERROR;
}

bool FSocketSubsystemAccelByte::IsSocketWaitSupported() const
{
	return false;
}

TSharedRef<FInternetAddr> FSocketSubsystemAccelByte::CreateInternetAddr()
{
	return MakeShareable(new FInternetAddrAccelByte());
}