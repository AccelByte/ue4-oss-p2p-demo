// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#include "AccelByteNetworkManager.h"
#include "AccelByteWebRTCMixedReality.h"
#include "HAL/UnrealMemory.h"
#include "AccelByteSignalingBase.h"
#include "AccelByteP2PBase.h"
#include "AccelByteJuice.h"
#include "AccelByteWebSocket.h"

struct P2PData 
{
	FString NetId;
	TArray<uint8> Data;
	P2PData(const FString& NetId, const TArray<uint8>& Data) :NetId(NetId), Data(Data) 
	{
	}
};

static AccelByteNetworkManager AccelByteNetworkManagerInstance;

AccelByteNetworkManager& AccelByteNetworkManager::Instance() 
{
	return AccelByteNetworkManagerInstance;
}

bool AccelByteNetworkManager::RequestConnect(const FString& UserId) 
{
	if (AllAccelByteWebRTC.Contains(UserId)) 
	{
		return false;
	}
	auto Rtc = CreateNewConnection(UserId);
	return Rtc->RequestConnect();
}

bool AccelByteNetworkManager::SendTo(const uint8* Data, int32 Count, int32& BytesSent, const FString& NetId) 
{
	if (!AllAccelByteWebRTC.Contains(NetId)) 
	{
		return false;
	}
	auto Rtc = AllAccelByteWebRTC[NetId];
	return Rtc->Send(Data, Count, BytesSent);
}

bool AccelByteNetworkManager::RecvFrom(uint8* Data, int32 BufferSize, int32& BytesRead, FString& NetId) 
{
	bool Ret = false;
	if (Offset < 0) 
	{
		Ret = QueueDatas.Dequeue(LastReadedData);
		if (Ret) 
		{
			Offset = 0;
		}
		else 
		{
			return false;
		}
	}
	auto Num = LastReadedData->Data.Num();
	if (Num > 0) 
	{
		Ret = true;
		NetId = LastReadedData->NetId;
		int32 Off = Offset;
		auto RealNum = Num - Offset;
		if (Offset + BufferSize >= Num) 
		{
			BytesRead = Num - Offset;
			Offset = -1;
		}
		else 
		{
			Offset += BufferSize;
			BytesRead = BufferSize;
		}
		FMemory::Memcpy(Data, LastReadedData->Data.GetData() + Off, BytesRead);
	}
	return Ret;
}

bool AccelByteNetworkManager::HasPendingData(uint32& PendingDataSize) 
{
	if (LastReadedData.IsValid()) 
	{
		PendingDataSize = LastReadedData->Data.Num();
		return true;
	}
	auto Item = QueueDatas.Peek();
	if (Item != nullptr) 
	{
		PendingDataSize = Item->Get()->Data.Num();
		return true;
	}
	PendingDataSize = 0;
	return false;
}

void AccelByteNetworkManager::ClosePeerConnection(const FString& NetId) 
{
	if (AllAccelByteWebRTC.Contains(NetId)) 
	{
		AllAccelByteWebRTC[NetId]->ClosePeerConnection();
	}
}

void AccelByteNetworkManager::CloseAllPeer() 
{
	for (const auto& pair : AllAccelByteWebRTC) 
	{
		pair.Value->ClosePeerConnection();
	}
}

void AccelByteNetworkManager::Run() 
{
	bIsRunning = true;
#if USE_SIMPLE_SIGNALING
	Signaling = MakeShareable<AccelByteSignalingBase>(AccelByteWebSocket::Instance());
#endif

	if(!Signaling->IsConnected())
	{
		Signaling->Connect();
	}
	
	auto Delegate = AccelByteSignalingBase::OnWebRTCSignalingMessage::CreateRaw(this, &AccelByteNetworkManager::OnSignalingMessage);
	Signaling->SetOnWebRTCSignalingMessage(Delegate);
}

void AccelByteNetworkManager::Tick(float DeltaTime) 
{
}

void AccelByteNetworkManager::OnSignalingMessage(const FString& UserId, const FString& Message) 
{
	if (!AllAccelByteWebRTC.Contains(UserId)) 
	{
		CreateNewConnection(UserId);
	}
	AllAccelByteWebRTC[UserId]->OnSignalingMessage(Message);
}

void AccelByteNetworkManager::IncomingData(const FString& From, const uint8* Data, int32 Count) 
{
	QueueDatas.Enqueue(MakeShared<P2PData>(From, TArray<uint8>((uint8*)Data, Count)));
}

void AccelByteNetworkManager::RTCConnected(const FString& NetId) 
{
	OnWebRTCDataChannelConnectedDelegate.ExecuteIfBound(NetId);
}

void AccelByteNetworkManager::RTCClosed(const FString& NetId) 
{
	if (AllAccelByteWebRTC.Contains(NetId)) 
	{
		AllAccelByteWebRTC.Remove(NetId);
		OnWebRTCDataChannelClosedDelegate.ExecuteIfBound(NetId);
	}
}

TSharedPtr<AccelByteP2PBase> AccelByteNetworkManager::CreateNewConnection(const FString& NetId) 
{
#ifdef MR_WEBRTC
	auto Rtc = MakeShared<AccelByteWebRTCMixedReality>(NetId);
#elif defined(LIBJUICE)
	auto Rtc = MakeShared<AccelByteJuice>(NetId);
#else
	auto Rtc = MakeShared<AccelByteP2PDummy>(NetId);
#endif
	Rtc->SetSignaling(Signaling.Get());
	Rtc->SetOnP2PDataChannelConnectedDelegate(AccelByteP2PBase::OnP2PDataChannelConnected::CreateRaw(this, &AccelByteNetworkManager::RTCConnected));
	Rtc->SetOnP2PDataChannelClosedDelegate(AccelByteP2PBase::OnP2PDataChannelClosed::CreateRaw(this, &AccelByteNetworkManager::RTCClosed));
	AllAccelByteWebRTC.Add(NetId, Rtc);
	Rtc->SetOnP2PDataReadyDelegate(AccelByteP2PBase::OnP2PDataReady::CreateRaw(this, &AccelByteNetworkManager::IncomingData));
	return Rtc;
}