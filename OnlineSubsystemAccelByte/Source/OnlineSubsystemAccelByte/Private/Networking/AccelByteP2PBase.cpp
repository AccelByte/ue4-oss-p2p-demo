// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#include "AccelByteP2PBase.h"
#include "Serialization/JsonSerializer.h"

void AccelByteP2PBase::SetSignaling(AccelByteSignalingBase* Value)
{
	Signaling = Value;
}

void AccelByteP2PBase::JsonToString(FString& Out, TSharedRef<FJsonObject> JsonObject)
{
	auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(JsonObject, Writer);
}

TSharedPtr<FJsonObject> AccelByteP2PBase::StringToJson(const FString& JsonString)
{
	TSharedPtr<FJsonObject> JsonOut = MakeShared<FJsonObject>();
    auto Reader = TJsonReaderFactory<TCHAR>::Create(JsonString);
    FJsonSerializer::Deserialize(Reader, JsonOut);
    return JsonOut;
}
