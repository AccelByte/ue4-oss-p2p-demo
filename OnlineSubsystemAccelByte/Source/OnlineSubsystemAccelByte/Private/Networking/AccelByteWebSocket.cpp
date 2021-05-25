// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#if USE_SIMPLE_SIGNALING

#include "AccelByteWebSocket.h"
#include "WebSocketsModule.h"
#include "OnlineSubsystemAccelByteLog.h"

static AccelByteWebSocket AccelByteWebSocketInstance;

void JsonToString(FString& Out, TSharedPtr<FJsonObject> JsonObject)
{
	auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
}

TSharedPtr<FJsonObject> StringToJson(const FString& JsonString)
{
	TSharedPtr<FJsonObject> JsonOut = MakeShared<FJsonObject>();
	auto Reader = TJsonReaderFactory<TCHAR>::Create(JsonString);
	FJsonSerializer::Deserialize(Reader, JsonOut);
	return JsonOut;
}

AccelByteWebSocket* AccelByteWebSocket::Instance()
{
	return &AccelByteWebSocketInstance;
}

bool AccelByteWebSocket::Init()
{
	auto guid = FGuid::NewGuid();
	Id = *guid.ToString();
	UE_LOG_AB(Log, TEXT("Init websocket with id: %s"), *Id);
	if (!FModuleManager::Get().IsModuleLoaded("WebSockets")) {
		FModuleManager::Get().LoadModule("WebSockets");
	}
	FString SignalingURL;
	GConfig->GetString(TEXT("OnlineSubsystemAccelByte"), TEXT("SignalingServerURL"), SignalingURL, GEngineIni);
	UE_LOG_AB(Log, TEXT("WebSocket URL: %s"), *SignalingURL);
	WebSocket = FWebSocketsModule::Get().CreateWebSocket(FString::Printf(TEXT("%s/%s"), *SignalingURL, *guid.ToString()), TEXT("wss"));
	WebSocket->OnConnected().AddRaw(this, &AccelByteWebSocket::OnConnected);
	WebSocket->OnClosed().AddRaw(this, &AccelByteWebSocket::OnClose);
	WebSocket->OnMessage().AddRaw(this, &AccelByteWebSocket::OnMessage);
	WebSocket->OnRawMessage().AddRaw(this, &AccelByteWebSocket::OnRawMessage);
	return true;
}

bool AccelByteWebSocket::IsConnected()
{
	if(WebSocket.IsValid()) return WebSocket->IsConnected();
	return false;
}

void AccelByteWebSocket::Connect()
{
	WebSocket->Connect();
}

void AccelByteWebSocket::SendMessage(const FString& PeerId, const FString& Msg)
{
	if(WebSocket.IsValid() && WebSocket->IsConnected())
	{
		auto Message = MakeShared<FJsonObject>();
		Message->SetStringField(TEXT("type"), TEXT("signaling"));
		Message->SetStringField(TEXT("id"), PeerId);
		Message->SetStringField(TEXT("message"), Msg);
		FString Out;
		JsonToString(Out, Message);
		SendMessage(Out);
	}
}

void AccelByteWebSocket::QueryGameSession()
{
	auto Message = MakeShared<FJsonObject>();
	Message->SetStringField(TEXT("type"), TEXT("listGameSession"));
	FString Out;
	JsonToString(Out, Message);
	SendMessage(Out);
}

void AccelByteWebSocket::InsertGameSession(TSharedPtr<FJsonObject> SessionJson)
{
	auto Message = MakeShared<FJsonObject>();
	Message->SetStringField(TEXT("type"), TEXT("createGameSession"));
	Message->SetObjectField(TEXT("session"), SessionJson);
	FString Out;
	JsonToString(Out, Message);
	SendMessage(Out);
}

void AccelByteWebSocket::JoinGameSession(const FString& PeerId)
{
	auto Message = MakeShared<FJsonObject>();
	Message->SetStringField(TEXT("type"), TEXT("joinGameSession"));
	Message->SetStringField(TEXT("id"), PeerId);
	FString Out;
	JsonToString(Out, Message);
	SendMessage(Out);
}

void AccelByteWebSocket::OnConnected()
{
	UE_LOG_AB(Log, TEXT("WebSocket connected"));
	OnWebSocketConnectedDelegate.ExecuteIfBound(Id);
}

void AccelByteWebSocket::OnClose(int32 StatusCode, const FString& Reason, bool bWasClean)
{
	UE_LOG_AB(Log, TEXT("WebSocket closed"));
}

void AccelByteWebSocket::OnMessage(const FString& Message)
{
	UE_LOG_AB(Log, TEXT("New message: %s"), *Message);
	auto Json = StringToJson(Message);
	auto Type = Json->GetStringField(TEXT("type"));
	if(Type.Equals(TEXT("createGameSession")))
	{
		OnGameSessionCreatedDelegate.ExecuteIfBound(true);
	}
	else if(Type.Equals(TEXT("listGameSession")))
	{
		auto GameSessions = Json->GetArrayField(TEXT("games"));
		OnGameSessionQueryDelegate.ExecuteIfBound(GameSessions);
	}
	else if(Type.Equals(TEXT("joinGameSession")))
	{
		auto Status = Json->GetBoolField(TEXT("status"));
		auto PeerId = Json->GetStringField(TEXT("peerId"));
		OnGameSessionJoinDelegate.ExecuteIfBound(Status, PeerId);
	}
	else
	{
		//This is signaling message
		auto From = Json->GetStringField(TEXT("id"));
		auto Msg = Json->GetStringField(TEXT("message"));
		OnWebRtcSignalingMessageDelegate.ExecuteIfBound(From, Msg);
	}
}

void AccelByteWebSocket::OnRawMessage(const void* Data, SIZE_T Size, SIZE_T BytesRemaining)
{
	UE_LOG_AB(Verbose, TEXT("New raw message size: %llu"), Size);
}

void AccelByteWebSocket::SendMessage(const FString& Message)
{
	if(WebSocket.IsValid() && WebSocket->IsConnected())
	{
		WebSocket->Send(Message);
	}
}

#endif