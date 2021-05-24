// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#pragma once

class AccelByteP2PBase;
class AccelByteSignalingBase;

struct P2PData;


/**
 * @brief Manager for network connection
 */
class AccelByteNetworkManager 
{

public:

	DECLARE_DELEGATE_OneParam(OnWebRTCDataChannelConnected, const FString&);
	DECLARE_DELEGATE_OneParam(OnWebRTCDataChannelClosed, const FString&);

	/**
	 * @brief Singleton instance of the manager
	 *
	*/
	static AccelByteNetworkManager& Instance();

	/**
	 * @brief Request connect to peer connection id
	 *
	 * @param UserId id of the remote peer connection
	*/
	bool RequestConnect(const FString& UserId);

	/**
	 * @brief Run the manager
	 *
	*/
	void Run();

	/**
	 * @brief Tick for the manager
	 *
	*/
	void Tick(float DeltaTime);

	/**
	 * @brief Send data to peer id connection
	 *
	 * @param Data to send
	 * @param Count data length to send
	 * @param BytesSent actual data sent
	 * @param NetId peer id of the remote
	*/
	bool SendTo(const uint8* Data, int32 Count, int32& BytesSent, const FString &NetId);

	/**
	 * @brief Receive data from any peer when any data available
	 *
	 * @param Data the storage of the data to store to
	 * @param BufferSize length of the data to read
	 * @param BytesRead actual data length read
	 * @param NetId peer id where the data come from
	*/
	bool RecvFrom(uint8* Data, int32 BufferSize, int32& BytesRead, FString &NetId);

	/**
	 * @brief Check if any data available to read
	 *
	 * @param PendingDataSize to store the length of available data to read
	 * 
	 * @return true if any data available
	*/
	bool HasPendingData(uint32& PendingDataSize);

	/**
	 * @brief Close the peer connection of specific peer id
	 *
	 * @param NetId peer id to close the connection
	*/
	void ClosePeerConnection(const FString& NetId);

	/**
	 * @brief Close all peer connection
	*/
	void CloseAllPeer();

	OnWebRTCDataChannelConnected OnWebRTCDataChannelConnectedDelegate;
	OnWebRTCDataChannelClosed OnWebRTCDataChannelClosedDelegate;

private:
	bool bIsRunning = false;
	TSharedPtr<AccelByteSignalingBase> Signaling;
	TMap<FString, TSharedPtr<AccelByteP2PBase>> AllAccelByteWebRTC;
	TQueue<TSharedPtr<P2PData>> QueueDatas;
	TSharedPtr<P2PData> LastReadedData;
	int Offset = -1;

	void OnSignalingMessage(const FString& UserId, const FString& Message);
	void IncomingData(const FString &From, const uint8* Data, int32 Count);
	void RTCConnected(const FString& NetId);
	void RTCClosed(const FString& NetId);
	TSharedPtr<AccelByteP2PBase> CreateNewConnection(const FString& NetId);
};