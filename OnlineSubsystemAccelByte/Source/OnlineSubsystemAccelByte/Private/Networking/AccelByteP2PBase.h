// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#pragma once

class AccelByteSignalingBase;

class AccelByteP2PBase
{
public:
	DECLARE_DELEGATE_OneParam(OnP2PDataChannelConnected, const FString&);
	DECLARE_DELEGATE_OneParam(OnP2PDataChannelClosed, const FString&);
	DECLARE_DELEGATE_ThreeParams(OnP2PDataReady, const FString&, const uint8*, int32);

	AccelByteP2PBase() 
	{ 
	}
	
	AccelByteP2PBase(const FString &PeerId): PeerId(PeerId) 
	{ 
	}
	
	virtual ~AccelByteP2PBase() 
	{
	}
	
	/**
	* @brief Process signaling message
	*
	* @param Message from signaling service (AccelByte Lobby)
	*/
	virtual void OnSignalingMessage(const FString& Message) = 0;

	/**
	* @brief Request connect to PeerId
	*/
	virtual bool RequestConnect() = 0;

	/**
	* @brief Send data to connected peer data channel
	*
	* @param Data to sent
	* @param Count of the data to be sent
	* @param BytesSent notify byte sent.
	* 
	* @return true when success
	*/
	virtual bool Send(const uint8* Data, int32 Count, int32& BytesSent) = 0;

	/**
	* @brief Set the signaling messenger
	*
	* @param Value the instance of the signaling messenger
	*/
	void SetSignaling(AccelByteSignalingBase *Value);

	/**
	* @brief Disconnect peer connection
	*
	*/
	virtual void ClosePeerConnection() = 0;

	/**
	* @brief Set delegate for P2P Connected
	*
	* @param Delegate
	*/
	void SetOnP2PDataChannelConnectedDelegate(OnP2PDataChannelConnected Delegate)
	{
		OnP2PDataChannelConnectedDelegate = Delegate;
	}

	/**
	* @brief Set delegate for P2P Closed
	*
	* @param Delegate
	*/
	void SetOnP2PDataChannelClosedDelegate(OnP2PDataChannelClosed Delegate)
	{
		OnP2PDataChannelClosedDelegate = Delegate;
	}

	/**
	* @brief Set delegate for P2P data ready to read
	*
	* @param Delegate
	*/
	void SetOnP2PDataReadyDelegate(OnP2PDataReady Delegate)
	{
		OnP2PDataReadyDelegate = Delegate;
	}

protected:
	AccelByteSignalingBase *Signaling;
	bool bIsInitiator = false;
	bool bIsConnected = false;
	bool bPeerReady = false;
	FString PeerId;
	OnP2PDataChannelConnected OnP2PDataChannelConnectedDelegate;
	OnP2PDataChannelClosed OnP2PDataChannelClosedDelegate;
	OnP2PDataReady OnP2PDataReadyDelegate;

	void JsonToString(FString& Out, TSharedRef<FJsonObject> JsonObject);
	TSharedPtr<FJsonObject> StringToJson(const FString& JsonString);
};

class AccelByteP2PDummy : public AccelByteP2PBase
{
public:
	AccelByteP2PDummy()
	{
	}
	
	AccelByteP2PDummy(const FString &PeerId)
	{
	}
	
	virtual ~AccelByteP2PDummy()
	{
	}
	
	virtual void OnSignalingMessage(const FString& Message) override
	{
	}
	
	virtual bool RequestConnect() override
	{
		return true;
	}
	
	virtual bool Send(const uint8* Data, int32 Count, int32& BytesSent) override
	{
		return true;
	}
	
	virtual void ClosePeerConnection() override
	{
	}
};