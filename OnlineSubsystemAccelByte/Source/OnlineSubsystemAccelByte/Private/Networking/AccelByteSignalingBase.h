// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#pragma once

class AccelByteSignalingBase
{
public:
	DECLARE_DELEGATE_TwoParams(OnWebRTCSignalingMessage, const FString&, const FString &);

	virtual ~AccelByteSignalingBase() 
	{
	}

	/**
	* @brief Init to init the signaling 
	*
	* @return true if success
	*/
	virtual bool Init() = 0;

	/**
	* @brief Check if signaling is connected
	*
	* @return true if connected;
	*/
	virtual bool IsConnected() = 0;

	/**
	* @brief Connect to signaling server
	*
	*/
	virtual void Connect() = 0;

	/**
	* @brief SendMessage to signaling server
	*
	* @param PeerId destination of peer to sent
	* @param Message to send
	*/
	virtual void SendMessage(const FString &PeerId, const FString &Message) = 0;

	/**
	* @brief Set the delegate when any data from signaling message
	*
	* @param Delegate with type OnWebRTCSignalingMessage
	*/
	void SetOnWebRTCSignalingMessage(OnWebRTCSignalingMessage Delegate) 
	{ 
		OnWebRtcSignalingMessageDelegate = Delegate;
	}

protected:
	OnWebRTCSignalingMessage OnWebRtcSignalingMessageDelegate;
};