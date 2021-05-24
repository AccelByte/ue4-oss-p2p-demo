// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#pragma once

#if USE_SIMPLE_SIGNALING

#include "AccelByteSignalingBase.h"
#include "IWebSocket.h" 

class AccelByteWebSocket : public AccelByteSignalingBase
{
public:
	DECLARE_DELEGATE_OneParam(OnWebSocketConnected, const FString&);
	DECLARE_DELEGATE_TwoParams(OnGameSessionJoin, bool, const FString&);
	DECLARE_DELEGATE_OneParam(OnGameSessionCreated, bool);
	DECLARE_DELEGATE_OneParam(OnGameSessionQuery, TArray<TSharedPtr<FJsonValue>>);
	
	AccelByteWebSocket() {}
	virtual ~AccelByteWebSocket() override {}

	static AccelByteWebSocket* Instance();
	
	virtual bool Init() override;
	virtual bool IsConnected() override;
	virtual void Connect() override;
	virtual void SendMessage(const FString &PeerId, const FString &Message) override;

	//Handle the game session
	void QueryGameSession();
	void InsertGameSession(TSharedPtr<FJsonObject> SessionJson);
	void JoinGameSession(const FString &PeerId);

	OnWebSocketConnected OnWebSocketConnectedDelegate;
	OnGameSessionCreated OnGameSessionCreatedDelegate;
	OnGameSessionJoin OnGameSessionJoinDelegate;
	OnGameSessionQuery OnGameSessionQueryDelegate;

private:
	void OnConnected();
	void OnClose(int32 StatusCode, const FString& Reason, bool bWasClean);
	void OnMessage(const FString& Message);
	void OnRawMessage(const void* Data, SIZE_T Size, SIZE_T BytesRemaining);
	void SendMessage(const FString &Message);

	TSharedPtr<IWebSocket> WebSocket;
	FString Id;
};

#endif