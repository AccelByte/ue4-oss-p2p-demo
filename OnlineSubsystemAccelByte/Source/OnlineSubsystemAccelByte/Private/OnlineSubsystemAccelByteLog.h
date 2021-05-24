// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(AccelByteOSS, Log, All);

#define UE_LOG_AB(Verbosity, Format, ...) \
{ \
	UE_LOG(AccelByteOSS, Verbosity, TEXT("%s"), *FString::Printf(Format, ##__VA_ARGS__)); \
}