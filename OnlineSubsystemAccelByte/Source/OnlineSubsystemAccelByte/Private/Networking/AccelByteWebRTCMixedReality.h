// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#pragma once

#ifdef MR_WEBRTC

#include "mrwebrtc/interop_api.h"
#include "mrwebrtc/data_channel_interop.h"
#include "AccelByteWebRTCBase.h"

/**
 * @brief AccelByte WebRTC connection handler.
 */
class AccelByteWebRTCMixedReality : public AccelByteWebRTCBase 
{
public:

	DECLARE_DELEGATE_OneParam(OnWebRTCDataChannelConnected, const FString&);
	DECLARE_DELEGATE_OneParam(OnWebRTCDataChannelClosed, const FString&);

	AccelByteWebRTCMixedReality(const FString &PeerId);

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
	mrsPeerConnectionHandle RTCHandle = nullptr;
	mrsDataChannelHandle DataHandle = nullptr;
	TQueue<TSharedPtr<FJsonObject>> SdpQueue;
	TQueue<TSharedPtr<FJsonObject>> CandidateQueue;
	bool bSdpApplied = false;

	bool CreatePeerConnection(const FString& IceAddress);
	void SetupCallback();
	void SetupDataCallback();
	void HandleMessage(TSharedPtr<FJsonObject> Json);

	//~ Begin mrRTC callback
	void PeerConnectedCallback();
	void PeerConnectionDataChannelAddedCallback(const mrsDataChannelAddedInfo* info);
	void PeerConnectionDataChannelRemovedCallback(mrsDataChannelHandle data_channel);
	void PeerConnectionLocalScpReadyCallback(mrsSdpMessageType type, const char* sdp_data);
	void PeerConnectionIceCandidateReadyCallback(const mrsIceCandidate* candidate);
	void PeerLocalDescriptionAppliedCallback(mrsResult result, const char* error_message);
	void DataChannelMessageCallback(const void* data, const uint64_t size);
	void DataChannelStateCallback(mrsDataChannelState state, int32_t id);
	void PeerIceDisconnected();
	void PeerNeedRenegotiation();
	//~ End mrRTC callback
};

#endif