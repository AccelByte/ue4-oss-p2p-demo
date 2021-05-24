// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#include "OnlineSubsystemAccelByteModule.h"
#include "OnlineSubsystemModule.h"
#include "OnlineSubsystemAccelByte.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemAccelByteLog.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CommandLine.h" 

DEFINE_LOG_CATEGORY(AccelByteOSS);

void* LibP2PHandle = nullptr;

IMPLEMENT_MODULE(FOnlineSubsystemAccelByteModule, OnlineSubsystemAccelByte);

class FOnlineFactoryAccelByte : public IOnlineFactory
{
public:

	FOnlineFactoryAccelByte() {}
	virtual ~FOnlineFactoryAccelByte() {}

	virtual IOnlineSubsystemPtr CreateSubsystem(FName InstanceName) override
	{
		FOnlineSubsystemAccelBytePtr OnlineSub = MakeShared<FOnlineSubsystemAccelByte, ESPMode::ThreadSafe>(InstanceName);
		if (OnlineSub->IsEnabled())
		{
			if (!OnlineSub->Init())
			{
				UE_LOG_AB(Warning, TEXT("AccelByte API failed to initialize!"));
				OnlineSub->Shutdown();
				OnlineSub = nullptr;
			}
		}
		else
		{
			UE_LOG_AB(Warning, TEXT("AccelByte API disabled!"));
			OnlineSub->Shutdown();
			OnlineSub = nullptr;
		}

		return OnlineSub;
	}
};

void FOnlineSubsystemAccelByteModule::StartupModule()
{
	FString ParamValue;
	if(FParse::Value(FCommandLine::Get(), TEXT("noaccelbyte"), ParamValue))
	{
		bOssAvailable = false;
		return;
	}
	const FString BaseDir =
		IPluginManager::Get().FindPlugin("OnlineSubsystemAccelByte")->GetBaseDir();
	
#ifdef MR_WEBRTC
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	const auto LibraryPath = FPaths::Combine(
		*BaseDir, TEXT("source/ThirdParty/MRWebRTC/x64/debug/"
			"mrwebrtc.dll"));
#else
	const auto LibraryPath = FPaths::Combine(
        *BaseDir, TEXT("source/ThirdParty/MRWebRTC/x64/release/"
            "mrwebrtc.dll"));
#endif
	LibP2PHandle = FPlatformProcess::GetDllHandle(*LibraryPath);
#endif

#ifdef LIBJUICE
#if PLATFORM_WINDOWS
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	const auto LibraryPath = FPaths::Combine(
            *BaseDir, TEXT("source/ThirdParty/LibJuice/x64/debug/juice.dll"));
#else
	const auto LibraryPath = FPaths::Combine(
            *BaseDir, TEXT("source/ThirdParty/LibJuice/x64/release/juice.dll"));
#endif
	LibP2PHandle = FPlatformProcess::GetDllHandle(*LibraryPath);
#elif PLATFORM_XSX
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	const auto LibraryPath = FPaths::Combine(
			*BaseDir, TEXT("source/ThirdParty/LibJuice/xsx/debug/juice.dll"));
#else
	const auto LibraryPath = FPaths::Combine(
			*BaseDir, TEXT("source/ThirdParty/LibJuice/xsx/release/juice.dll"));
#endif
	//TODO: dynamic library not working
	//LibP2PHandle = FPlatformProcess::GetDllHandle(LibraryPath);
#elif PLATFORM_PS4
    LibP2PHandle = FPlatformProcess::GetDllHandle(TEXT("juice.prx"));
#elif PLATFORM_PS5
	LibP2PHandle = FPlatformProcess::GetDllHandle(TEXT("juice.prx"));
#endif //PLATFORM_WINDOWS
#endif //LIBJUICE

	// Create and register our singleton factory with the main online subsystem for easy access
	FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
	AccelByteFactory = new FOnlineFactoryAccelByte();
	OSS.RegisterPlatformService(ACCELBYTE_SUBSYSTEM, AccelByteFactory);
	UE_LOG_AB(Log, TEXT("AccelByte OSS startup"));
}

void FOnlineSubsystemAccelByteModule::ShutdownModule()
{
	if(!bOssAvailable)
	{
		return;
	}	

	FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
	OSS.UnregisterPlatformService(ACCELBYTE_SUBSYSTEM);

	if(AccelByteFactory)
	{
		delete AccelByteFactory;
		AccelByteFactory = nullptr;		
	}

	if(LibP2PHandle)
	{
		FPlatformProcess::FreeDllHandle(LibP2PHandle);
		LibP2PHandle = nullptr;		
	} 
	
	UE_LOG_AB(Log, TEXT("AccelByte OSS shutdown"));
}
