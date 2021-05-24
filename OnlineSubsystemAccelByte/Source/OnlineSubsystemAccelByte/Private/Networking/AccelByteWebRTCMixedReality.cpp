// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#ifdef MR_WEBRTC

#include "AccelByteWebRTCMixedReality.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Core/AccelByteRegistry.h"
#include "Api/AccelByteLobbyApi.h"
#include "OnlineSubsystemAccelByteLog.h"
#include "Async/TaskGraphInterfaces.h" 
#include "Misc/Base64.h"
#include "mrwebrtc/ref_counted_object_interop.h"

//Helper to hold unapplied ice and candidates

struct SdpDescription
{
	FString Type;
	FString SdpType;
	FString Sdp;
};

struct IceCandidate
{
};

void JsonToString(FString& Out, TSharedRef<FJsonObject> JsonObject)
{
	auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(JsonObject, Writer);
}

TSharedPtr<FJsonObject> StringToJson(const FString& JsonString)
{
	TSharedPtr<FJsonObject> JsonOut = MakeShared<FJsonObject>();
	auto Reader = TJsonReaderFactory<TCHAR>::Create(JsonString);
	FJsonSerializer::Deserialize(Reader, JsonOut);
	return JsonOut;
}

AccelByteWebRTCMixedReality::AccelByteWebRTCMixedReality(const FString& PeerId):AccelByteWebRTCBase(PeerId) 
{
}

void AccelByteWebRTCMixedReality::OnSignalingMessage(const FString& Message) 
{
	FString Base64Decoded;
	FBase64::Decode(Message, Base64Decoded);
	UE_LOG_AB(Log, TEXT("Message : %s"), *Base64Decoded);
	auto Json = StringToJson(Base64Decoded);
	HandleMessage(Json);
}

bool AccelByteWebRTCMixedReality::RequestConnect() 
{
	bIsInitiator = true;
	FString TurnServerUrl;
	GConfig->GetString(TEXT("OnlineSubsystemAccelByte"), TEXT("TurnServerURL"), TurnServerUrl, GEngineIni);
	auto Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("type"), TEXT("ice"));
	Json->SetStringField(TEXT("server"), TurnServerUrl);
	Json->SetStringField(TEXT("server_type"), TEXT("offer"));
	if (AccelByte::FRegistry::Lobby.IsConnected())
	{
		FString JsonString;
		JsonToString(JsonString, Json);
		FString Base64String = FBase64::Encode(JsonString);
		AccelByte::FRegistry::Lobby.SendSignalingMessage(PeerId, Base64String);
		FFunctionGraphTask::CreateAndDispatchWhenReady([this, stunServer]() {
			CreatePeerConnection(stunServer);
		}, TStatId(), nullptr);
		return true;
	}
	return false;
}

bool AccelByteWebRTCMixedReality::Send(const uint8* Data, int32 Count, int32& BytesSent) 
{
	if (!bIsConnected)
	{
		return false;
	}
	auto Result = mrsDataChannelSendMessage(DataHandle, Data, Count);
	BytesSent = Count;
	return Result == mrsResult::kSuccess;
}

void AccelByteWebRTCMixedReality::ClosePeerConnection() 
{
	UE_LOG_AB(Log, TEXT("Closing peer connection to : "), *PeerId);
	if (RTCHandle != nullptr)
	{
		mrsPeerConnectionClose(RTCHandle);
		RTCHandle = nullptr;
		DataHandle = nullptr;
	}
}

bool AccelByteWebRTCMixedReality::CreatePeerConnection(const FString& IceAddress) 
{
	UE_LOG_AB(Log, TEXT("Create peer connection to : %s"), *IceAddress);
	mrsPeerConnectionConfiguration config{
	  TCHAR_TO_ANSI(*IceAddress), mrsIceTransportType::kAll,
	  mrsBundlePolicy::kBalanced, mrsSdpSemantic::kUnifiedPlan };
	auto Result = mrsPeerConnectionCreate(&config, &RTCHandle);
	UE_LOG_AB(Log, TEXT("Create peer result : %d"), Result);
	bool Success = Result == mrsResult::kSuccess;
	if (Success)
	{
		bPeerReady = true;
		SetupCallback();
		TSharedPtr<FJsonObject> Json;
		if (SdpQueue.Dequeue(Json))
		{
			HandleMessage(Json);
		}
	}
	return Success;
}

void AccelByteWebRTCMixedReality::SetupCallback() 
{
	mrsPeerConnectionRegisterConnectedCallback(RTCHandle, [](void* user_data) {
		auto Owner = static_cast<AccelByteWebRTCMixedReality*>(user_data);
		Owner->PeerConnectedCallback();
	}, this);
	mrsPeerConnectionRegisterLocalSdpReadytoSendCallback(RTCHandle, [](void* user_data, mrsSdpMessageType type, const char* sdp_data) {
		auto Owner = static_cast<AccelByteWebRTCMixedReality*>(user_data);
		Owner->PeerConnectionLocalScpReadyCallback(type, sdp_data);
	}, this);
	mrsPeerConnectionRegisterIceCandidateReadytoSendCallback(RTCHandle, [](void* user_data, const mrsIceCandidate* candidate) {
		auto Owner = static_cast<AccelByteWebRTCMixedReality*>(user_data);
		Owner->PeerConnectionIceCandidateReadyCallback(candidate);
	}, this);
	mrsPeerConnectionRegisterIceStateChangedCallback(RTCHandle, [](void* user_data, mrsIceConnectionState new_state) {
		UE_LOG_AB(Log, TEXT("WebRTC ICE state change: %d"), new_state);
		if (new_state == mrsIceConnectionState::kDisconnected) {
			auto Owner = static_cast<AccelByteWebRTCMixedReality*>(user_data);
			Owner->PeerIceDisconnected();
		}
	}, this);
	mrsPeerConnectionRegisterDataChannelAddedCallback(RTCHandle, [](void* user_data, const mrsDataChannelAddedInfo* info) {
		auto Owner = static_cast<AccelByteWebRTCMixedReality*>(user_data);
		Owner->PeerConnectionDataChannelAddedCallback(info);
	}, this);
	mrsPeerConnectionRegisterDataChannelRemovedCallback(RTCHandle, [](void* user_data, mrsDataChannelHandle data_channel) {
		auto Owner = static_cast<AccelByteWebRTCMixedReality*>(user_data);
		Owner->PeerConnectionDataChannelRemovedCallback(data_channel);
	}, this);
	mrsPeerConnectionRegisterRenegotiationNeededCallback(RTCHandle, [](void *user_data)
	{
		auto Owner = static_cast<AccelByteWebRTCMixedReality*>(user_data);
		Owner->PeerNeedRenegotiation();
	}, this);
	if (bIsInitiator) {
		mrsDataChannelConfig config{ -1, mrsDataChannelConfigFlags::kNone, "AccelByteData" };
		auto Result = mrsPeerConnectionAddDataChannel(RTCHandle, &config, &DataHandle);
		UE_LOG_AB(Log, TEXT("WebRTC add data channel: %d"), Result);
		/*if (Result == mrsResult::kSuccess) {
			Result = mrsPeerConnectionCreateOffer(RTCHandle);
			UE_LOG_AB(Log, TEXT("WebRTC OFFER : %d"), Result);
		}*/
	}
}

void AccelByteWebRTCMixedReality::SetupDataCallback() 
{
	mrsDataChannelSetUserData(DataHandle, this);
	mrsDataChannelCallbacks callback{
		[](void* user_data, const void* data, const uint64_t size) {
			auto Owner = static_cast<AccelByteWebRTCMixedReality*>(user_data);
			Owner->DataChannelMessageCallback(data, size);
		},
		this,
		nullptr,
		nullptr,
		[](void* user_data, mrsDataChannelState state, int32_t id) {
			auto Owner = static_cast<AccelByteWebRTCMixedReality*>(user_data);
			Owner->DataChannelStateCallback(state, id);
		},
		this,
	};
	mrsDataChannelRegisterCallbacks(DataHandle, &callback);	
}

void AccelByteWebRTCMixedReality::HandleMessage(TSharedPtr<FJsonObject> Json) 
{
	auto Type = Json->GetStringField(TEXT("type"));
	if (Type.Equals(TEXT("ice")))
	{
		auto ServerType = Json->GetStringField(TEXT("server_type"));
		if (ServerType.Equals(TEXT("offer"))) {
			auto StunServer = Json->GetStringField(TEXT("server"));
			FFunctionGraphTask::CreateAndDispatchWhenReady([this, StunServer]() {
				CreatePeerConnection(StunServer);
				auto Json = MakeShared<FJsonObject>();
				Json->SetStringField(TEXT("type"), TEXT("ice"));
				Json->SetStringField(TEXT("server_type"), TEXT("answer"));
				FString JsonString;
				JsonToString(JsonString, Json);
				FString Base64String = FBase64::Encode(JsonString);
				AccelByte::FRegistry::Lobby.SendSignalingMessage(PeerId, Base64String);
			}, TStatId(), nullptr);
		}
	}
	else if (Type.Equals(TEXT("sdp")))
	{
		auto SdpType = Json->GetStringField(TEXT("sdp_type"));
		auto SdpBase64 = Json->GetStringField(TEXT("sdp"));
		FString Sdp;
		FBase64::Decode(SdpBase64, Sdp);
		UE_LOG_AB(Log, TEXT("Ser remote description : %s"), *Sdp);
		if (bPeerReady) {
			mrsPeerConnectionSetRemoteDescriptionAsync(RTCHandle, SdpType.Equals(TEXT("offer")) ? mrsSdpMessageType::kOffer : mrsSdpMessageType::kAnswer,
				TCHAR_TO_ANSI(*Sdp), [](void* user_data, mrsResult result, const char* error_message) {
				auto Owner = static_cast<AccelByteWebRTCMixedReality*>(user_data);
				Owner->PeerLocalDescriptionAppliedCallback(result, error_message);
			}, this);
		}
		else {
			SdpQueue.Enqueue(Json);
		}
	}
	else if (Type.Equals(TEXT("candidate")))
	{
		if(bSdpApplied)
		{
			auto index = Json->GetIntegerField(TEXT("index"));
			auto SdpMidBase64 = Json->GetStringField(TEXT("sdp_mid"));
			auto ContentBase64 = Json->GetStringField(TEXT("content"));
			FString SdpMid;
			FString Content;
			FBase64::Decode(SdpMidBase64, SdpMid);
			FBase64::Decode(ContentBase64, Content);
			mrsIceCandidate candidate{ StringCast<ANSICHAR>(*SdpMid).Get(), StringCast<ANSICHAR>(*Content).Get(), index };
			UE_LOG_AB(Log, TEXT("Ser candidate : %d : %s : %s"), index, *SdpMid, *Content);
			mrsPeerConnectionAddIceCandidate(RTCHandle, &candidate);
		} else
		{
			CandidateQueue.Enqueue(Json);
		}
	}
}

void AccelByteWebRTCMixedReality::PeerConnectedCallback() 
{
	UE_LOG_AB(Log, TEXT("WebRTC peer CONNECTED!!!"));
}

void AccelByteWebRTCMixedReality::PeerConnectionDataChannelAddedCallback(const mrsDataChannelAddedInfo* info) 
{
	UE_LOG_AB(Log, TEXT("WebRTC channel data added"));
	DataHandle = info->handle;	
	SetupDataCallback();
}

void AccelByteWebRTCMixedReality::PeerConnectionDataChannelRemovedCallback(mrsDataChannelHandle data_channel) 
{
	UE_LOG_AB(Log, TEXT("WebRTC channel data removed"));
}

void AccelByteWebRTCMixedReality::PeerConnectionLocalScpReadyCallback(mrsSdpMessageType type, const char* sdp_data) 
{
	UE_LOG_AB(Log, TEXT("WebRTC Local SDP Ready"));
	auto Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("type"), TEXT("sdp"));
	Json->SetStringField(TEXT("sdp_type"), type == mrsSdpMessageType::kOffer ? TEXT("offer") : TEXT("answer"));
	Json->SetStringField(TEXT("sdp"), FBase64::Encode(FString(sdp_data)));
	FString JsonString;
	JsonToString(JsonString, Json);
	FString Base64String = FBase64::Encode(JsonString);
	AccelByte::FRegistry::Lobby.SendSignalingMessage(PeerId, Base64String);
}

void AccelByteWebRTCMixedReality::PeerConnectionIceCandidateReadyCallback(const mrsIceCandidate* candidate) 
{
	UE_LOG_AB(Log, TEXT("WebRTC Candidate ready: %d : %s : %s"), candidate->sdp_mline_index, candidate->sdp_mid, candidate->content);
	auto Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("type"), TEXT("candidate"));
	Json->SetNumberField(TEXT("index"), candidate->sdp_mline_index);
	Json->SetStringField(TEXT("sdp_mid"), FBase64::Encode(FString(candidate->sdp_mid)));
	Json->SetStringField(TEXT("content"), FBase64::Encode(FString(candidate->content)));
	FString JsonString;
	JsonToString(JsonString, Json);
	FString Base64String = FBase64::Encode(JsonString);
	AccelByte::FRegistry::Lobby.SendSignalingMessage(PeerId, Base64String);
}

void AccelByteWebRTCMixedReality::PeerLocalDescriptionAppliedCallback(mrsResult result, const char* error_message) 
{
	if (result == mrsResult::kSuccess)
	{
		mrsPeerConnectionCreateAnswer(RTCHandle);
		bSdpApplied = true;
		TSharedPtr<FJsonObject> Json;
		if (CandidateQueue.Dequeue(Json)) {
			HandleMessage(Json);
		}
	}
	else
		{
		UE_LOG_AB(Log, TEXT("Set local description error: %s"), *FString(error_message));
	}
}

void AccelByteWebRTCMixedReality::DataChannelMessageCallback(const void* data, const uint64_t size) 
{
	OnWebRTCDataReadyDelegate.ExecuteIfBound(PeerId, (uint8*)data, size);
}

void AccelByteWebRTCMixedReality::DataChannelStateCallback(mrsDataChannelState state, int32_t id) 
{
	UE_LOG_AB(Log, TEXT("Data channel state changed: %d"), state);
	if (state == mrsDataChannelState::kOpen)
	{
		bIsConnected = true;
		OnWebRTCDataChannelConnectedDelegate.ExecuteIfBound(PeerId);
	}
	else if (state == mrsDataChannelState::kClosed)
	{
		bIsConnected = false;
		OnWebRTCDataChannelClosedDelegate.ExecuteIfBound(PeerId);
	}
}

void AccelByteWebRTCMixedReality::PeerIceDisconnected() 
{
	bIsConnected = false;
	RTCHandle = nullptr;
	DataHandle = nullptr;
	OnWebRTCDataChannelClosedDelegate.ExecuteIfBound(PeerId);
}

void AccelByteWebRTCMixedReality::PeerNeedRenegotiation()
{
	UE_LOG_AB(Log, TEXT("WebRTC need renegotiation"));
	mrsPeerConnectionCreateOffer(RTCHandle);
}

#endif
