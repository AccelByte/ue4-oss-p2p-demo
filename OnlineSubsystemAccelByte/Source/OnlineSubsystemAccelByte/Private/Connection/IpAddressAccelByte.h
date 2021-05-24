// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#pragma once

#include "IPAddress.h"
#include "OnlineSubsystemAccelByteTypes.h"
#include "OnlineSubsystemAccelBytePackage.h"

class FInternetAddrAccelByte : public FInternetAddr
{
PACKAGE_SCOPE:
	FUniqueNetIdAccelByte NetId;

	FInternetAddrAccelByte(const FInternetAddrAccelByte& Src);

public:
	FInternetAddrAccelByte();
	FInternetAddrAccelByte(const FUniqueNetIdAccelByte& InSteamId);

	//~ Begin FInternetAddr Interface
	virtual TArray<uint8> GetRawIp() const override;
	virtual void SetRawIp(const TArray<uint8>& RawAddr) override;
	void SetIp(uint32 InAddr) override;
	void SetIp(const TCHAR* InAddr, bool& bIsValid) override;
	void GetIp(uint32& OutAddr) const override;
	void SetPort(int32 InPort) override {};
	void GetPort(int32& OutPort) const override;
	int32 GetPort() const override;
	void SetAnyAddress() override {};
	void SetBroadcastAddress() override {};
	void SetLoopbackAddress() override {};
	FString ToString(bool bAppendPort) const override;
	virtual bool operator==(const FInternetAddr& Other) const override;
	bool operator!=(const FInternetAddrAccelByte& Other) const;
	virtual uint32 GetTypeHash() const override;	
	virtual bool IsValid() const override;
	virtual FName GetProtocolType() const override;
	virtual TSharedRef<FInternetAddr> Clone() const override;
	//~ End FInternetAddr Interface
};
