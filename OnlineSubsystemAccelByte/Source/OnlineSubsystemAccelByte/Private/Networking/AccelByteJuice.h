// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#pragma once

#ifdef LIBJUICE

#include "AccelByteP2PBase.h"
#include "juice/juice.h"

class AccelByteJuice: public AccelByteP2PBase 
{
public:
	DECLARE_DELEGATE_OneParam(OnJuiceConnected, const FString&);
	DECLARE_DELEGATE_OneParam(OnJuiceClosed, const FString&);

	AccelByteJuice(const FString &PeerId);

	/**
	* @brief Process signaling message
	*
	* @param Message from signaling service (AccelByte Lobby)
	*/
	virtual void OnSignalingMessage(const FString& Message) override;

	/**
	* @brief Request connect to PeerId
	*/
	virtual bool RequestConnect() override;

	/**
	* @brief Send data to connected peer data channel
	*
	* @param Data to sent
	* @param Count of the data to be sent
	* @param BytesSent notify byte sent.
	* 
	* @return true when success
	*/
	virtual bool Send(const uint8* Data, int32 Count, int32& BytesSent) override;

	/**
	* @brief Disconnect peer connection
	*
	*/
	virtual void ClosePeerConnection() override;

private:
	juice_agent_t *JuiceAgent = nullptr;
	juice_config_t JuiceConfig;
	bool bIsDescriptionReady = false;
	TQueue<TSharedPtr<FJsonObject>> SdpQueue;
	TQueue<TSharedPtr<FJsonObject>> CandidateQueue;

	bool CreatePeerConnection(const FString& Host, const FString &UserName, const FString &Password, int port);
	void SetupCallback();
	void HandleMessage(TSharedPtr<FJsonObject> Json);
	void SetupLocalDescription();

	// juice callback
	void JuiceStateChanged(juice_state_t State);
	void JuiceCandidate(const char *Candidate);
	void JuiceGatheringDone();
	void JuiceDataRecv(const char *data, size_t size);
};

#endif