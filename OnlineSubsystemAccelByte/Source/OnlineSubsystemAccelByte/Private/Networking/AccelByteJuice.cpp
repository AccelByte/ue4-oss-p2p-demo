// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#ifdef LIBJUICE

#include "AccelByteJuice.h"
#include "Dom/JsonObject.h"
#include "OnlineSubsystemAccelByteLog.h"
#include "Async/TaskGraphInterfaces.h" 
#include "Misc/Base64.h"
#include "AccelByteSignalingBase.h"

#define DO_TASK(task) FFunctionGraphTask::CreateAndDispatchWhenReady(task, TStatId(), nullptr);

static void LogCallback(juice_log_level_t level, const char *message) {
	UE_LOG_AB(Verbose, TEXT("JUICE LOG : %hs"), message);
}

AccelByteJuice::AccelByteJuice(const FString& PeerId): AccelByteP2PBase(PeerId)
{
	juice_set_log_handler(LogCallback);
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	juice_set_log_level(JUICE_LOG_LEVEL_VERBOSE);
#else
	juice_set_log_level(JUICE_LOG_LEVEL_NONE);
#endif
}

void AccelByteJuice::OnSignalingMessage(const FString& Message)
{
	FString Base64Decoded;
	FBase64::Decode(Message, Base64Decoded);
	UE_LOG_AB(Verbose, TEXT("Signaling Message : %s"), *Base64Decoded);
	auto Json = StringToJson(Base64Decoded);
	HandleMessage(Json);
}

bool AccelByteJuice::RequestConnect()
{
	bIsInitiator = true;	
	FString TurnHost;
	int TurnPort = 3478;
	FString TurnUserName;
	FString TurnPassword;
	GConfig->GetString(TEXT("OnlineSubsystemAccelByte"), TEXT("TurnServerUrl"), TurnHost, GEngineIni);
	GConfig->GetInt(TEXT("OnlineSubsystemAccelByte"), TEXT("TurnServerPort"), TurnPort, GEngineIni);
	GConfig->GetString(TEXT("OnlineSubsystemAccelByte"), TEXT("TurnServerUsername"), TurnUserName, GEngineIni);
	GConfig->GetString(TEXT("OnlineSubsystemAccelByte"), TEXT("TurnServerPassword"), TurnPassword, GEngineIni);
	auto Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("type"), TEXT("ice"));
	Json->SetStringField(TEXT("host"), TurnHost);
	Json->SetStringField(TEXT("username"), TurnUserName);
	Json->SetStringField(TEXT("password"), TurnPassword);
	Json->SetNumberField(TEXT("port"), TurnPort);	
	Json->SetStringField(TEXT("server_type"), TEXT("offer"));
	if (Signaling->IsConnected())
	{
		FString JsonString;
		JsonToString(JsonString, Json);
		FString Base64String = FBase64::Encode(JsonString);
		Signaling->SendMessage(PeerId, Base64String);
		DO_TASK(([this, TurnHost, TurnUserName, TurnPassword, TurnPort]() {
            if(CreatePeerConnection(TurnHost, TurnUserName, TurnPassword, TurnPort))
            {
                SetupLocalDescription();
            }
        }));
		return true;
	}
	return false;
}

bool AccelByteJuice::Send(const uint8* Data, int32 Count, int32& BytesSent)
{
	if (!bIsConnected || JuiceAgent == nullptr)
	{
		return false;
	}
	int Ret = juice_send(JuiceAgent, (char*)Data, Count);
	BytesSent = Count;
	return Ret == JUICE_ERR_SUCCESS;
}

void AccelByteJuice::ClosePeerConnection()
{
	bIsConnected = false;
	if(JuiceAgent != nullptr)
	{
		auto ToDestroy = JuiceAgent;
		JuiceAgent = nullptr;
		juice_destroy(ToDestroy);		
	}	
}

bool AccelByteJuice::CreatePeerConnection(const FString& Host, const FString &UserName, const FString &Password, int port)
{
	if(!Host.IsEmpty())
	{
		juice_turn_server TurnServerConfig[1];
		auto AnsiHost = StringCast<ANSICHAR>(*Host);
		auto AnsiUser = StringCast<ANSICHAR>(*UserName);
		auto AnsiPassword = StringCast<ANSICHAR>(*Password);
		TurnServerConfig[0].host = AnsiHost.Get();
		TurnServerConfig[0].port = port;
		TurnServerConfig[0].username = AnsiUser.Get();
		TurnServerConfig[0].password = AnsiPassword.Get();	
		JuiceConfig.turn_servers = TurnServerConfig;
		JuiceConfig.turn_servers_count = 1; 
	}	
	memset(&JuiceConfig, 0, sizeof(JuiceConfig));
	SetupCallback();
	FString StunHost; 
	int StunPort;
	if(GConfig->GetString(TEXT("OnlineSubsystemAccelByte"), TEXT("StunServerUrl"), StunHost, GEngineIni)
		&& GConfig->GetInt(TEXT("OnlineSubsystemAccelByte"), TEXT("StunPort"), StunPort, GEngineIni)
		&& StunHost.IsEmpty())
	{
		auto AnsiStunHost = StringCast<ANSICHAR>(*StunHost);
		JuiceConfig.stun_server_host = AnsiStunHost.Get();
		JuiceConfig.stun_server_port = StunPort;
	}
	JuiceConfig.user_ptr = this;
#if PLATFORM_PS4
	JuiceConfig.local_port_range_begin = 10000;
	JuiceConfig.local_port_range_end = 10010;
#endif
	JuiceAgent = juice_create(&JuiceConfig);
	bPeerReady = JuiceAgent != nullptr;
	return bPeerReady;
}

void AccelByteJuice::SetupCallback()
{
	JuiceConfig.cb_state_changed = [](juice_agent_t *agent, juice_state_t state, void *user_ptr)
	{
		auto Owner = static_cast<AccelByteJuice*>(user_ptr);
		Owner->JuiceStateChanged(state);
	};
	JuiceConfig.cb_candidate = [](juice_agent_t *agent, const char *sdp, void *user_ptr)
	{
		auto Owner = static_cast<AccelByteJuice*>(user_ptr);
		Owner->JuiceCandidate(sdp);
	};
	JuiceConfig.cb_recv = [](juice_agent_t *agent, const char *data, size_t size, void *user_ptr)
	{
		auto Owner = static_cast<AccelByteJuice*>(user_ptr);
		Owner->JuiceDataRecv(data, size);
	};
	JuiceConfig.cb_gathering_done = [](juice_agent_t *agent, void *user_ptr)
	{
		auto Owner = static_cast<AccelByteJuice*>(user_ptr);
		Owner->JuiceGatheringDone();
	};
}

void AccelByteJuice::HandleMessage(TSharedPtr<FJsonObject> Json)
{
	auto Type = Json->GetStringField(TEXT("type"));
	FString JsonString;
	JsonToString(JsonString, Json.ToSharedRef());
	UE_LOG_AB(Log, TEXT("Signaling Message : %s"), *JsonString);
	if (Type.Equals(TEXT("ice")))
	{
		auto ServerType = Json->GetStringField(TEXT("server_type"));
		if (ServerType.Equals(TEXT("offer"))) {
			auto Host = Json->GetStringField(TEXT("host"));
			auto Username = Json->GetStringField(TEXT("username"));
			auto Password = Json->GetStringField(TEXT("password"));
			auto Port = Json->GetIntegerField(TEXT("port"));
			FFunctionGraphTask::CreateAndDispatchWhenReady([this, Host, Username, Password, Port]() {
				if(CreatePeerConnection(Host, Username, Password, Port))
				{
					SetupLocalDescription();
					TSharedPtr<FJsonObject> Json;
			        if (SdpQueue.Dequeue(Json))
			        {
			            HandleMessage(Json);
			        }
				}
			}, TStatId(), nullptr);
		}
	}
	else if (Type.Equals(TEXT("sdp")))
	{
		auto Sdp = Json->GetStringField(TEXT("sdp"));		
		if(bPeerReady)
		{
			DO_TASK(([this, Sdp]()
			{
				juice_set_remote_description(JuiceAgent, TCHAR_TO_ANSI(*Sdp));
				UE_LOG_AB(Log, TEXT("Set remote description : %s"), *Sdp);
				int Result = juice_gather_candidates(JuiceAgent);
				UE_LOG_AB(Log, TEXT("Gather Candidate : %d"), Result);
				bIsDescriptionReady = true;
			}));
			
			TSharedPtr<FJsonObject> JsonCandidate;
			while (CandidateQueue.Dequeue(JsonCandidate))
			{
				HandleMessage(JsonCandidate);
			}
		} else
		{
			SdpQueue.Enqueue(Json);
		}
		
	}
	else if (Type.Equals(TEXT("candidate")))
	{
		if(bIsDescriptionReady)
		{			
			auto Candidate = Json->GetStringField(TEXT("candidate"));
			UE_LOG_AB(Log, TEXT("Set remote candidate : %s"), *Candidate);
			DO_TASK(([this, Candidate]()
			{
				juice_add_remote_candidate(JuiceAgent, TCHAR_TO_ANSI(*Candidate));				
			}));
		} else
		{
			CandidateQueue.Enqueue(Json);
		}		
	}
}

void AccelByteJuice::SetupLocalDescription()
{
	char Buffer[JUICE_MAX_SDP_STRING_LEN];
	int Ret = juice_get_local_description(JuiceAgent, Buffer, JUICE_MAX_SDP_STRING_LEN);
	UE_LOG_AB(Log, TEXT("Juice Local Description : %d : %hs"), Ret, Buffer);
	auto Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("type"), TEXT("sdp"));
	Json->SetStringField(TEXT("sdp"), FString(Buffer));
	FString JsonString;
	JsonToString(JsonString, Json);
	FString Base64String = FBase64::Encode(JsonString);
	Signaling->SendMessage(PeerId, Base64String);
}

void AccelByteJuice::JuiceStateChanged(juice_state_t State)
{
	UE_LOG_AB(Log, TEXT("Juice state changed : %d"), State);
	if(State == JUICE_STATE_CONNECTED)
	{
		bIsConnected = true;
		OnP2PDataChannelConnectedDelegate.ExecuteIfBound(PeerId);
	} else if(State == JUICE_STATE_FAILED || State == JUICE_STATE_DISCONNECTED)
	{
		bIsConnected = false;
		juice_destroy(JuiceAgent);
		JuiceAgent = nullptr;
		OnP2PDataChannelClosedDelegate.ExecuteIfBound(PeerId);
	}
}

void AccelByteJuice::JuiceCandidate(const char* Candidate)
{
	UE_LOG_AB(Log, TEXT("Juice local candidate : %hs"), Candidate);
	auto Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("type"), TEXT("candidate"));
	Json->SetStringField(TEXT("candidate"), FString(Candidate));
	FString JsonString;
	JsonToString(JsonString, Json);
	FString Base64String = FBase64::Encode(JsonString);
	Signaling->SendMessage(PeerId, Base64String);
}

void AccelByteJuice::JuiceGatheringDone()
{
	UE_LOG_AB(Log, TEXT("Juice Gathering Done"));
}

void AccelByteJuice::JuiceDataRecv(const char* data, size_t size)
{
	OnP2PDataReadyDelegate.ExecuteIfBound(PeerId, (uint8*)data, size);
}

#endif