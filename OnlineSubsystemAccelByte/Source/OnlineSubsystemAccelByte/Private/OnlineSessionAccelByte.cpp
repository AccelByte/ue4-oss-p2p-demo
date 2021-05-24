// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#include "OnlineSessionAccelByte.h"
#include "OnlineSubsystemAccelByteTypes.h"
#include "OnlineSubsystemAccelByte.h"
#include "OnlineSubsystemAccelByteLog.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "SocketSubsystem.h"
#include "OnlineAsyncTaskManager.h"
#include "Networking/AccelByteNetworkManager.h"
#include "Misc/DefaultValueHelper.h"
#include "OnlineKeyValuePair.h"
#include "Networking/AccelByteWebSocket.h"

bool GetConnectionStringFromSessionInfo(TSharedPtr<FOnlineSessionInfoAccelByte> SessionInfo, FString& ConnectInfo, int32 PortOverride = 0)
{
	if (SessionInfo->RemoteId.IsEmpty()) {
		int32 HostPort = SessionInfo->HostAddr->GetPort();
		if (PortOverride > 0)
		{
			HostPort = PortOverride;
		}
		ConnectInfo = FString::Printf(TEXT("%s:%d"), *SessionInfo->HostAddr->ToString(false), HostPort);
		return true;
	}
	else 
	{
		ConnectInfo = FString::Printf(ACCELBYTE_URL_PREFIX TEXT("%s:11223"), *SessionInfo->RemoteId);
		return true;
	}
	return false;
}

TSharedPtr<const FUniqueNetId> FOnlineSessionAccelByte::CreateSessionIdFromString(const FString& SessionIdStr)
{
	TSharedPtr<const FUniqueNetId> SessionId;
	if (!SessionIdStr.IsEmpty())
	{
		SessionId = MakeShared<FUniqueNetIdAccelByte>(SessionIdStr);
	}
	return SessionId;
}

FNamedOnlineSession* FOnlineSessionAccelByte::GetNamedSession(FName SessionName)
{
	FScopeLock ScopeLock(&SessionLock);
	for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
	{
		if (Sessions[SearchIndex].SessionName == SessionName)
		{
			return &Sessions[SearchIndex];
		}
	}
	return nullptr;
}

void FOnlineSessionAccelByte::RemoveNamedSession(FName SessionName)
{
	FScopeLock ScopeLock(&SessionLock);
	for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
	{
		if (Sessions[SearchIndex].SessionName == SessionName)
		{
			Sessions.RemoveAtSwap(SearchIndex);
			return;
		}
	}
}

EOnlineSessionState::Type FOnlineSessionAccelByte::GetSessionState(FName SessionName) const
{
	FScopeLock ScopeLock(&SessionLock);
	for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
	{
		if (Sessions[SearchIndex].SessionName == SessionName)
		{
			return Sessions[SearchIndex].SessionState;
		}
	}

	return EOnlineSessionState::NoSession;
}

bool FOnlineSessionAccelByte::HasPresenceSession()
{
	FScopeLock ScopeLock(&SessionLock);
	for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
	{
		if (Sessions[SearchIndex].SessionSettings.bUsesPresence)
		{
			return true;
		}
	}

	return false;
}

bool FOnlineSessionAccelByte::CreateSession(int32 HostingPlayerNum, FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
	uint32 Result = ONLINE_FAIL;

	if (!NewSessionSettings.bIsLANMatch && !NewSessionSettings.bIsDedicated && !AccelByteWebSocket::Instance()->IsConnected()) {
		UE_LOG_AB(Warning, TEXT("Create game session failed because not connected to lobby! Session name: %s"), *SessionName.ToString());
		TriggerOnCreateSessionCompleteDelegates(LastSessionCreated, false);
		return false;
	}

	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session == nullptr)
	{
		Session = AddNamedSession(SessionName, NewSessionSettings);
		check(Session);
		Session->SessionState = EOnlineSessionState::Creating;
		Session->NumOpenPrivateConnections = NewSessionSettings.NumPrivateConnections;
		Session->NumOpenPublicConnections = NewSessionSettings.NumPublicConnections;

		Session->HostingPlayerNum = HostingPlayerNum;

		check(AccelByteSubsystem);
		IOnlineIdentityPtr Identity = AccelByteSubsystem->GetIdentityInterface();
		if (Identity.IsValid())
		{
			Session->OwningUserId = Identity->GetUniquePlayerId(HostingPlayerNum);
			Session->OwningUserName = Identity->GetPlayerNickname(HostingPlayerNum);
		}

		//TODO: this should not working (throw error), if the login is mandatory
		if (!Session->OwningUserId.IsValid())
		{
			Session->OwningUserId = MakeShareable(new FUniqueNetIdAccelByte(FString::Printf(TEXT("%d"), HostingPlayerNum)));
			Session->OwningUserName = FString(TEXT("AccelByteUser"));
		}

		// Unique identifier of this build for compatibility
		Session->SessionSettings.BuildUniqueId = GetBuildUniqueId();

		// Setup the host session info
		auto NewSessionInfo = new FOnlineSessionInfoAccelByte();
		NewSessionInfo->Init(*AccelByteSubsystem);
		Session->SessionInfo = MakeShareable(NewSessionInfo);	

		LastSessionCreated = SessionName;
		Session->SessionState = EOnlineSessionState::Pending;
		if(NewSessionSettings.bIsLANMatch || NewSessionSettings.bIsDedicated)
		{
			Result = UpdateLANStatus();
			if(Result != ONLINE_SUCCESS)
			{
				RemoveNamedSession(SessionName);
			}
		} else
		{
			Result = ONLINE_IO_PENDING;			
			CreateSessionBrowser(Session);
		}
	}
	else
	{
		UE_LOG_AB(Warning, TEXT("Cannot create session '%s': session already exists."), *SessionName.ToString());
	}

	if (Result != ONLINE_IO_PENDING)
	{
		TriggerOnCreateSessionCompleteDelegates(SessionName, (Result == ONLINE_SUCCESS) ? true : false);
	}

	return Result == ONLINE_IO_PENDING || Result == ONLINE_SUCCESS;
}

bool FOnlineSessionAccelByte::CreateSession(const FUniqueNetId& HostingPlayerId, FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
	return CreateSession(0, SessionName, NewSessionSettings);
}

void FOnlineSessionAccelByte::CreateSessionBrowser(FNamedOnlineSession* Session)
{
	const auto Setting = Session->SessionSettings;
	FString GameMode;
	FString GameMapName;
	const FString GameVersion = FString::Printf(TEXT("%d"), GetBuildUniqueId());
	int GameNumBot;
	Setting.Get(SETTING_GAMEMODE, GameMode);
	Setting.Get(SETTING_MAPNAME, GameMapName);
	Setting.Get(SETTING_NUMBOTS, GameNumBot);

	auto SettingJson = MakeShared<FJsonObject>();
	
	for(const auto &Set : Setting.Settings)
	{
		auto const &Data = Set.Value.Data;
		auto JsonShared = Set.Value.Data.ToJson();
		SettingJson->SetField(Set.Key.ToString(), JsonShared->TryGetField(TEXT("value")));
	}

#if USE_SIMPLE_SIGNALING
	if(!AccelByteWebSocket::Instance()->OnGameSessionCreatedDelegate.IsBound())
	{
		AccelByteWebSocket::Instance()->OnGameSessionCreatedDelegate.BindLambda([this](bool) 
		{
			FNamedOnlineSession* Session = GetNamedSession(LastSessionCreated);
		    auto AccelByteSessionInfo = StaticCastSharedPtr<FOnlineSessionInfoAccelByte>(Session->SessionInfo);
		    //AccelByteSessionInfo->SessionId = FUniqueNetIdAccelByte(Session->OwningUserId);
		    TriggerOnCreateSessionCompleteDelegates(LastSessionCreated, true);
		});
	}
	auto SessionJson = MakeShared<FJsonObject>();
	SessionJson->SetStringField(TEXT("GameMode"), GameMode);
	SessionJson->SetStringField(TEXT("GameMapName"), GameMapName);
	SessionJson->SetStringField(TEXT("OwningUserId"), Session->OwningUserId->ToString());
	SessionJson->SetStringField(TEXT("OwningUserName"), Session->OwningUserName);
	SessionJson->SetNumberField(TEXT("NumOpenPrivateConnections"), Session->NumOpenPrivateConnections);
	SessionJson->SetNumberField(TEXT("NumOpenPublicConnections"), Session->NumOpenPublicConnections);

	SessionJson->SetStringField(TEXT("SessionID"), Session->SessionInfo->GetSessionId().ToString());
	auto SessionSettingJson = MakeShared<FJsonObject>();
	SessionSettingJson->SetNumberField(TEXT("NumPublicConnections"), Setting.NumPublicConnections);
	SessionSettingJson->SetNumberField(TEXT("NumPrivateConnections"), Setting.NumPrivateConnections);
	SessionSettingJson->SetNumberField(TEXT("BuildUniqueId"), Setting.BuildUniqueId);
	SessionSettingJson->SetBoolField(TEXT("bShouldAdvertise"), Setting.bShouldAdvertise);
	SessionSettingJson->SetBoolField(TEXT("bIsLANMatch"), Setting.bIsLANMatch);
	SessionSettingJson->SetBoolField(TEXT("bIsDedicated"), Setting.bIsDedicated);
	SessionSettingJson->SetBoolField(TEXT("bUsesStats"), Setting.bUsesStats);
	SessionSettingJson->SetBoolField(TEXT("bAllowJoinInProgress"), Setting.bAllowJoinInProgress);
	SessionSettingJson->SetBoolField(TEXT("bAllowInvites"), Setting.bAllowInvites);
	SessionSettingJson->SetBoolField(TEXT("bUsesPresence"), Setting.bUsesPresence);
	SessionSettingJson->SetBoolField(TEXT("bAllowJoinViaPresence"), Setting.bAllowJoinViaPresence);
	SessionSettingJson->SetBoolField(TEXT("bIsLANMatch"), Setting.bIsLANMatch);
	SessionSettingJson->SetBoolField(TEXT("bAllowJoinViaPresenceFriendsOnly"), Setting.bAllowJoinViaPresenceFriendsOnly);
	SessionSettingJson->SetBoolField(TEXT("bAntiCheatProtected"), Setting.bAntiCheatProtected);

	SessionJson->SetObjectField(TEXT("SessionSetting"), SessionSettingJson);
	AccelByteWebSocket::Instance()->InsertGameSession(SessionJson);
#endif
}

bool FOnlineSessionAccelByte::StartSession(FName SessionName)
{
	uint32 Result = ONLINE_FAIL;
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		if (Session->SessionState == EOnlineSessionState::Pending ||
			Session->SessionState == EOnlineSessionState::Ended)
		{
			Session->SessionState = EOnlineSessionState::InProgress;
			Result = ONLINE_SUCCESS;
		}
		else
		{
			UE_LOG_AB(Warning, TEXT("Can't start an online session (%s) in state %s"),
				*SessionName.ToString(),
				EOnlineSessionState::ToString(Session->SessionState));
		}
	}
	else
	{
		UE_LOG_AB(Warning, TEXT("Can't start an online game for session (%s) that hasn't been created"), *SessionName.ToString());
	}

	if (Result != ONLINE_IO_PENDING)
	{
		TriggerOnStartSessionCompleteDelegates(SessionName, (Result == ONLINE_SUCCESS) ? true : false);
	}
	return Result == ONLINE_SUCCESS || Result == ONLINE_IO_PENDING;
}

bool FOnlineSessionAccelByte::UpdateSession(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings, bool bShouldRefreshOnlineData)
{
	return true;
}

bool FOnlineSessionAccelByte::EndSession(FName SessionName)
{
	uint32 Result = ONLINE_FAIL;

	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		//TODO: CHECK THIS
		/*if (Session->SessionState == EOnlineSessionState::InProgress)
		{
			AccelByte::FRegistry::SessionBrowser.RemoveGameSession(SessionBrowserData.Session_id,
				THandler<FAccelByteModelsSessionBrowserData>::CreateLambda([this, Session, SessionName](const FAccelByteModelsSessionBrowserData& Result) {
				UE_LOG_AB(Warning, TEXT("Remove session browser success"));
				Session->SessionState = EOnlineSessionState::Ended;
				TriggerOnEndSessionCompleteDelegates(SessionName, true);
			}), FErrorHandler::CreateLambda([this, SessionName](int32 ErrorCode, const FString& ErrorMessage) {
				UE_LOG_AB(Warning, TEXT("Error remove session browser %s"), *ErrorMessage);
				TriggerOnEndSessionCompleteDelegates(SessionName, false);
			}));
		}
		else
		{
			UE_LOG_AB(Warning, TEXT("Can't end session (%s) in state %s"),
				*SessionName.ToString(),
				EOnlineSessionState::ToString(Session->SessionState));
		}*/
	}
	else
	{
		UE_LOG_AB(Warning, TEXT("Can't end an online game for session (%s) that hasn't been created"),
			*SessionName.ToString());
	}
	return true;
}

bool FOnlineSessionAccelByte::DestroySession(FName SessionName, const FOnDestroySessionCompleteDelegate& CompletionDelegate) 
{
	uint32 Result = ONLINE_FAIL;
	// Find the session in question
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		RemoveNamedSession(Session->SessionName);
		Result = ONLINE_SUCCESS;
	}
	else
	{
		UE_LOG_AB(Warning, TEXT("Can't destroy a null online session (%s)"), *SessionName.ToString());
	}

	if (Result != ONLINE_IO_PENDING)
	{
		CompletionDelegate.ExecuteIfBound(SessionName, (Result == ONLINE_SUCCESS) ? true : false);
		TriggerOnDestroySessionCompleteDelegates(SessionName, (Result == ONLINE_SUCCESS) ? true : false);
	}
	return Result == ONLINE_SUCCESS || Result == ONLINE_IO_PENDING;
}

bool FOnlineSessionAccelByte::IsPlayerInSession(FName SessionName, const FUniqueNetId& UniqueId)
{
	return IsPlayerInSessionImpl(this, SessionName, UniqueId);
}

bool FOnlineSessionAccelByte::StartMatchmaking(const TArray< TSharedRef<const FUniqueNetId> >& LocalPlayers, FName SessionName, const FOnlineSessionSettings& NewSessionSettings, TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	//not implemented yet
	return false;
}

bool FOnlineSessionAccelByte::CancelMatchmaking(int32 SearchingPlayerNum, FName SessionName)
{
	//not implemented yet
	return false;
}

bool FOnlineSessionAccelByte::CancelMatchmaking(const FUniqueNetId& SearchingPlayerId, FName SessionName)
{
	//not implemented yet
	return false;
}

bool FOnlineSessionAccelByte::FindSessions(int32 SearchingPlayerNum, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	uint32 Return = ONLINE_FAIL;

	if (!CurrentSessionSearch.IsValid() && SearchSettings->SearchState != EOnlineAsyncTaskState::InProgress)
	{
		SearchSettings->SearchResults.Empty();

		CurrentSessionSearch = SearchSettings;

		SessionSearchStartInSeconds = FPlatformTime::Seconds();

		if(SearchSettings->bIsLanQuery) 
		{
			Return = FindLANSession();
		}
		else
		{
#if USE_SIMPLE_SIGNALING
			if(!AccelByteWebSocket::Instance()->OnGameSessionQueryDelegate.IsBound())
			{
				AccelByteWebSocket::Instance()->OnGameSessionQueryDelegate.BindRaw(this, &FOnlineSessionAccelByte::SessionFindResultLoaded);
			}
			AccelByteWebSocket::Instance()->QueryGameSession();
#endif
			Return = ONLINE_IO_PENDING;
		}
		if (Return == ONLINE_IO_PENDING)
		{
			SearchSettings->SearchState = EOnlineAsyncTaskState::InProgress;
		}
	}
	else
	{
		UE_LOG_AB(Warning, TEXT("Ignoring game search request while one is pending"));
		Return = ONLINE_IO_PENDING;
	}

	return Return == ONLINE_SUCCESS || Return == ONLINE_IO_PENDING;
}

void FOnlineSessionAccelByte::GetSessionBrowserTimeout()
{
	CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Done;
	CurrentSessionSearch = nullptr;
	TriggerOnFindSessionsCompleteDelegates(true);
}

void FOnlineSessionAccelByte::SessionFindResultLoaded(TArray<TSharedPtr<FJsonValue>> Values)
{
	if (CurrentSessionSearch.IsValid() && CurrentSessionSearch->SearchState == EOnlineAsyncTaskState::InProgress)
	{
		for(auto Value : Values)
		{
			FOnlineSessionSearchResult result;
			result.PingInMs = -1;
			auto JsonObject = Value->AsObject();
			auto JsonSession = JsonObject->GetObjectField(TEXT("SessionSetting"));
			auto JsonSessionSetting = JsonSession->GetObjectField(TEXT("SessionSetting"));
			FOnlineSession session;
			session.OwningUserName = JsonSession->GetStringField(TEXT("OwningUserName"));
			session.NumOpenPrivateConnections = JsonSession->GetNumberField(TEXT("NumOpenPrivateConnections"));
			session.NumOpenPublicConnections = JsonSession->GetNumberField(TEXT("NumOpenPublicConnections"));
			auto NetId = new FUniqueNetIdAccelByte(JsonSession->GetStringField(TEXT("OwningUserId")));
			session.OwningUserId = MakeShareable(NetId);
			session.SessionSettings.bAllowInvites = false;
			session.SessionSettings.bAllowJoinInProgress = true;
			session.SessionSettings.bAllowJoinViaPresence = JsonSessionSetting->GetBoolField(TEXT("bAllowJoinViaPresence"));
			session.SessionSettings.bAllowJoinViaPresenceFriendsOnly = false;
			session.SessionSettings.bAntiCheatProtected = true;
			session.SessionSettings.bIsDedicated = false;
			session.SessionSettings.bIsLANMatch = false;
			session.SessionSettings.bShouldAdvertise = false;
			session.SessionSettings.BuildUniqueId = JsonSessionSetting->GetNumberField(TEXT("BuildUniqueId"));
			session.SessionSettings.bUsesPresence = false;
			session.SessionSettings.bUsesStats = false;
			session.SessionSettings.NumPrivateConnections = 0;
			session.SessionSettings.NumPublicConnections = JsonSessionSetting->GetNumberField(TEXT("NumPublicConnections"));

			session.SessionSettings.Set(SETTING_GAMEMODE, JsonSession->GetStringField(TEXT("GameMode")));
			session.SessionSettings.Set(SETTING_MAPNAME, JsonSession->GetStringField(TEXT("GameMapName")));

			auto SesInfo = new FOnlineSessionInfoAccelByte;
			SesInfo->SessionId = FUniqueNetIdAccelByte(JsonSession->GetStringField(TEXT("SessionID")));
			SesInfo->RemoteId = JsonSession->GetStringField(TEXT("OwningUserId"));
			session.SessionInfo = MakeShareable(SesInfo);
			
			result.Session = session;
			CurrentSessionSearch->SearchResults.Add(result);
		}
		CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Done;
		CurrentSessionSearch = nullptr;
		TriggerOnFindSessionsCompleteDelegates(true);
	}
}

bool FOnlineSessionAccelByte::FindSessions(const FUniqueNetId& SearchingPlayerId, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	//only support single login in single instance of application
	return FindSessions(0, SearchSettings);;
}

bool FOnlineSessionAccelByte::FindSessionById(const FUniqueNetId& SearchingUserId, const FUniqueNetId& SessionId, const FUniqueNetId& FriendId, const FOnSingleSessionResultCompleteDelegate& CompletionDelegate)
{
	return false;
}

bool FOnlineSessionAccelByte::CancelFindSessions()
{
	uint32 Return = ONLINE_FAIL;
	if (CurrentSessionSearch.IsValid() && CurrentSessionSearch->SearchState == EOnlineAsyncTaskState::InProgress)
	{
		Return = ONLINE_SUCCESS;
		CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Failed;
		CurrentSessionSearch = nullptr;
	}
	else
	{
		UE_LOG_AB(Warning, TEXT("Can't cancel a search that isn't in progress"));
	}

	TriggerOnCancelFindSessionsCompleteDelegates(true);

	return true;
}

bool FOnlineSessionAccelByte::PingSearchResults(const FOnlineSessionSearchResult& SearchResult)
{
	//TODO: implement for LAN and dedicated
	return false;
}

bool FOnlineSessionAccelByte::JoinSession(int32 PlayerNum, FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
	uint32 Return = ONLINE_FAIL;
	UE_LOG_AB(Log, TEXT("Joining session: %s"), *SessionName.ToString());
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session == nullptr)
	{
		Session = AddNamedSession(SessionName, DesiredSession.Session);
		SessionToJoin = SessionName;
		const auto Info = static_cast<FOnlineSessionInfoAccelByte*>(DesiredSession.Session.SessionInfo.Get());
		if (!DesiredSession.Session.SessionSettings.bIsLANMatch && !DesiredSession.Session.SessionSettings.bIsDedicated)
		{
			if(AccelByteWebSocket::Instance()->IsConnected())
			{
				AccelByteNetworkManager::Instance().RequestConnect(Info->RemoteId);
				AccelByteNetworkManager::Instance().OnWebRTCDataChannelConnectedDelegate.BindRaw(this, &FOnlineSessionAccelByte::RTCConnected);
				Return = ONLINE_IO_PENDING;
			} else
			{
				UE_LOG_AB(Log, TEXT("Joining p2p session error: Lobby websocket is not connected!!!"));
				TriggerOnJoinSessionCompleteDelegates(SessionToJoin, EOnJoinSessionCompleteResult::CouldNotRetrieveAddress);
			}
		} else
		{
			FOnlineSessionInfoAccelByte* NewSessionInfo = new FOnlineSessionInfoAccelByte();
			Session->SessionInfo = MakeShareable(NewSessionInfo);
			Session->SessionSettings.bShouldAdvertise = false;
			Return = JoinLANSession(PlayerNum, Session, &DesiredSession.Session);
		}
	} else
	{
		UE_LOG_AB(Warning, TEXT("Session (%s) already exists, can't join twice"), *SessionName.ToString());
	}
	if (Return != ONLINE_IO_PENDING)
	{
		TriggerOnJoinSessionCompleteDelegates(SessionName, Return == ONLINE_SUCCESS ? EOnJoinSessionCompleteResult::Success : EOnJoinSessionCompleteResult::UnknownError);
	}
	return Return == ONLINE_SUCCESS || Return == ONLINE_IO_PENDING;
}

void FOnlineSessionAccelByte::RTCConnected(const FString& NetId)
{
	TriggerOnJoinSessionCompleteDelegates(SessionToJoin, EOnJoinSessionCompleteResult::Success);
}

bool FOnlineSessionAccelByte::JoinSession(const FUniqueNetId& PlayerId, FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
	//only support 1 login
	return JoinSession(0, SessionName, DesiredSession);
}

bool FOnlineSessionAccelByte::FindFriendSession(int32 LocalUserNum, const FUniqueNetId& Friend)
{
	//not implemented
	return false;
}

bool FOnlineSessionAccelByte::FindFriendSession(const FUniqueNetId& LocalUserId, const FUniqueNetId& Friend)
{
	//not implemented
	return false;
}

bool FOnlineSessionAccelByte::FindFriendSession(const FUniqueNetId& LocalUserId, const TArray<TSharedRef<const FUniqueNetId>>& FriendList)
{
	//not implemented
	return false;
}

bool FOnlineSessionAccelByte::SendSessionInviteToFriend(int32 LocalUserNum, FName SessionName, const FUniqueNetId& Friend)
{
	//not implemented
	return false;
}

bool FOnlineSessionAccelByte::SendSessionInviteToFriend(const FUniqueNetId& LocalUserId, FName SessionName, const FUniqueNetId& Friend)
{
	//not implemented
	return false;
}

bool FOnlineSessionAccelByte::SendSessionInviteToFriends(int32 LocalUserNum, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends)
{
	//not implemented
	return false;
}

bool FOnlineSessionAccelByte::SendSessionInviteToFriends(const FUniqueNetId& LocalUserId, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends)
{
	//not implemented
	return false;
}

bool FOnlineSessionAccelByte::GetResolvedConnectString(FName SessionName, FString& ConnectInfo, FName PortType)
{
	bool bSuccess = false;
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	TSharedPtr<FOnlineSessionInfoAccelByte> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoAccelByte>(Session->SessionInfo);
	if (PortType == NAME_BeaconPort)
	{
		int32 BeaconListenPort = GetBeaconPortFromSessionSettings(Session->SessionSettings);
		bSuccess = GetConnectionStringFromSessionInfo(SessionInfo, ConnectInfo, BeaconListenPort);
	}
	else if (PortType == NAME_GamePort)
	{
		bSuccess = GetConnectionStringFromSessionInfo(SessionInfo, ConnectInfo);
	}

	if (!bSuccess || ConnectInfo.IsEmpty())
	{
		UE_LOG_AB(Warning, TEXT("Invalid session info in search result to GetResolvedConnectString()"));
	}

	return bSuccess;
}

bool FOnlineSessionAccelByte::GetResolvedConnectString(const FOnlineSessionSearchResult& SearchResult, FName PortType, FString& ConnectInfo)
{
	bool bSuccess = false;
	if (SearchResult.Session.SessionInfo.IsValid())
	{
		TSharedPtr<FOnlineSessionInfoAccelByte> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoAccelByte>(SearchResult.Session.SessionInfo);

		if (PortType == NAME_BeaconPort)
		{
			int32 BeaconListenPort = GetBeaconPortFromSessionSettings(SearchResult.Session.SessionSettings);
			bSuccess = GetConnectionStringFromSessionInfo(SessionInfo, ConnectInfo, BeaconListenPort);
		}
		else if (PortType == NAME_GamePort)
		{
			bSuccess = GetConnectionStringFromSessionInfo(SessionInfo, ConnectInfo);
		}
	}

	if (!bSuccess || ConnectInfo.IsEmpty())
	{
		UE_LOG_AB(Warning, TEXT("Invalid session info in search result to GetResolvedConnectString()"));
	}

	return bSuccess;
}

FOnlineSessionSettings* FOnlineSessionAccelByte::GetSessionSettings(FName SessionName) 
{
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		return &Session->SessionSettings;
	}
	return nullptr;
}

bool FOnlineSessionAccelByte::RegisterPlayer(FName SessionName, const FUniqueNetId& PlayerId, bool bWasInvited)
{
	TArray< TSharedRef<const FUniqueNetId> > Players;
	Players.Add(MakeShareable(new FUniqueNetIdAccelByte(PlayerId)));
	return RegisterPlayers(SessionName, Players, bWasInvited);
}

bool FOnlineSessionAccelByte::RegisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players, bool bWasInvited)
{
	bool bSuccess = false;
    FNamedOnlineSession* Session = GetNamedSession(SessionName);
    if (Session)
    {
    	bSuccess = true;
    	for (int32 PlayerIdx=0; PlayerIdx<Players.Num(); PlayerIdx++)
    	{
    		const TSharedRef<const FUniqueNetId>& PlayerId = Players[PlayerIdx];
    		FUniqueNetIdMatcher PlayerMatch(*PlayerId);
    		if (Session->RegisteredPlayers.IndexOfByPredicate(PlayerMatch) == INDEX_NONE)
    		{
    			Session->RegisteredPlayers.Add(PlayerId);
    			if (Session->NumOpenPublicConnections > 0)
    			{
    				Session->NumOpenPublicConnections--;
    			}
    			else if (Session->NumOpenPrivateConnections > 0)
    			{
    				Session->NumOpenPrivateConnections--;
    			}
    			if(!Session->SessionSettings.bIsLANMatch && !Session->SessionSettings.bIsDedicated)
    			{
    				//TODO: CHECK THIS
    				/*AccelByte::FRegistry::SessionBrowser.UpdateGameSession(SessionBrowserData.Session_id, Session->SessionSettings.NumPublicConnections,
                    Session->RegisteredPlayers.Num(),
                    THandler<FAccelByteModelsSessionBrowserData>::CreateLambda([this, SessionName](const FAccelByteModelsSessionBrowserData& Result) {
                        UE_LOG_AB(Log, TEXT("Update player count success"));
	                }), FErrorHandler::CreateLambda([this, SessionName](int32 ErrorCode, const FString& ErrorMessage) {
	                    UE_LOG_AB(Warning, TEXT("Error update session browser %s"), *ErrorMessage);
	                }));*/
    			}    			
    		}
    		else
    		{
    			UE_LOG_AB(Log, TEXT("Player %s already registered in session %s"), *PlayerId->ToDebugString(), *SessionName.ToString());
    		}			
    	}
    }
    else
    {
    	UE_LOG_AB(Warning, TEXT("No game present to join for session (%s)"), *SessionName.ToString());
    }

    TriggerOnRegisterPlayersCompleteDelegates(SessionName, Players, bSuccess);
    return bSuccess;
}

bool FOnlineSessionAccelByte::UnregisterPlayer(FName SessionName, const FUniqueNetId& PlayerId)
{
	TArray< TSharedRef<const FUniqueNetId> > Players;
	Players.Add(MakeShareable(new FUniqueNetIdAccelByte(PlayerId)));
	return UnregisterPlayers(SessionName, Players);
}

bool FOnlineSessionAccelByte::UnregisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players)
{
	bool bSuccess = true;
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		for (int32 PlayerIdx=0; PlayerIdx < Players.Num(); PlayerIdx++)
		{
			const TSharedRef<const FUniqueNetId>& PlayerId = Players[PlayerIdx];

			FUniqueNetIdMatcher PlayerMatch(*PlayerId);
			int32 RegistrantIndex = Session->RegisteredPlayers.IndexOfByPredicate(PlayerMatch);
			if (RegistrantIndex != INDEX_NONE)
			{
				Session->RegisteredPlayers.RemoveAtSwap(RegistrantIndex);

				if (Session->NumOpenPublicConnections < Session->SessionSettings.NumPublicConnections)
				{
					Session->NumOpenPublicConnections++;
				}
				else if (Session->NumOpenPrivateConnections < Session->SessionSettings.NumPrivateConnections)
				{
					Session->NumOpenPrivateConnections++;
				}
				if(!Session->SessionSettings.bIsLANMatch && !Session->SessionSettings.bIsDedicated)
				{
					//TODO: CHECK THIS
					/* AccelByte::FRegistry::SessionBrowser.UpdateGameSession(SessionBrowserData.Session_id, Session->SessionSettings.NumPublicConnections,
                        Session->RegisteredPlayers.Num(),
                        THandler<FAccelByteModelsSessionBrowserData>::CreateLambda([this, SessionName](const FAccelByteModelsSessionBrowserData& Result) {
                            UE_LOG_AB(Log, TEXT("Update player count success"));
                    }), FErrorHandler::CreateLambda([this, SessionName](int32 ErrorCode, const FString& ErrorMessage) {
                        UE_LOG_AB(Warning, TEXT("Error update session browser %s"), *ErrorMessage);
                    }));*/
				}
			}
			else
			{
				UE_LOG_AB(Warning, TEXT("Player %s is not part of session (%s)"), *PlayerId->ToDebugString(), *SessionName.ToString());
			}
		}
	}
	else
	{
		UE_LOG_AB(Warning, TEXT("No game present to leave for session (%s)"), *SessionName.ToString());
		bSuccess = false;
	}

	TriggerOnUnregisterPlayersCompleteDelegates(SessionName, Players, bSuccess);
	return bSuccess;
}

void FOnlineSessionAccelByte::RegisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnRegisterLocalPlayerCompleteDelegate& Delegate)
{
	Delegate.ExecuteIfBound(PlayerId, EOnJoinSessionCompleteResult::Success);
}

void FOnlineSessionAccelByte::UnregisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnUnregisterLocalPlayerCompleteDelegate& Delegate)
{
	Delegate.ExecuteIfBound(PlayerId, true);
}

int32 FOnlineSessionAccelByte::GetNumSessions()
{
	FScopeLock ScopeLock(&SessionLock);
	return Sessions.Num();
}

void FOnlineSessionAccelByte::DumpSessionState()
{
	FScopeLock ScopeLock(&SessionLock);

	for (int32 SessionIdx = 0; SessionIdx < Sessions.Num(); SessionIdx++)
	{
		DumpNamedSession(&Sessions[SessionIdx]);
	}
}

void FOnlineSessionAccelByte::Tick(float DeltaTime)
{
	if (CurrentSessionSearch.IsValid() && !CurrentSessionSearch->bIsLanQuery && CurrentSessionSearch->SearchState == EOnlineAsyncTaskState::InProgress)
	{
		const auto Diff = FPlatformTime::Seconds() - SessionSearchStartInSeconds;
		if (Diff > 2) {
			GetSessionBrowserTimeout();
		}
	}
	LANSessionManager.Tick(DeltaTime);
}

bool FOnlineSessionAccelByte::NeedsAdvertising()
{
	FScopeLock ScopeLock(&SessionLock);
	for (int32 i=0; i < Sessions.Num(); i++)
	{
		FNamedOnlineSession& Session = Sessions[i];
		if (NeedsAdvertising(Session))
		{
			return true;
		}
	}
	return false;
}

bool FOnlineSessionAccelByte::NeedsAdvertising(const FNamedOnlineSession& Session)
{
	return Session.SessionSettings.bIsLANMatch && Session.SessionSettings.bShouldAdvertise && IsHost(Session);
}

bool FOnlineSessionAccelByte::IsSessionJoinable(const FNamedOnlineSession& Session) const
{
	const auto& Settings = Session.SessionSettings;
	const bool bIsAdvertising = Settings.bShouldAdvertise || Settings.bIsLANMatch;
	const bool bJoinableFromProgress = Session.SessionState != EOnlineSessionState::InProgress || Settings.bAllowJoinInProgress;
	return bIsAdvertising && bJoinableFromProgress && Session.NumOpenPublicConnections > 0;
}

uint32 FOnlineSessionAccelByte::UpdateLANStatus()
{
	uint32 Result = ONLINE_SUCCESS;
	if (NeedsAdvertising())
	{
		if (LANSessionManager.GetBeaconState() == ELanBeaconState::NotUsingLanBeacon)
		{
			FOnValidQueryPacketDelegate QueryPacketDelegate = FOnValidQueryPacketDelegate::CreateRaw(this, &FOnlineSessionAccelByte::OnValidQueryPacketReceived);
			if (!LANSessionManager.Host(QueryPacketDelegate))
			{
				Result = ONLINE_FAIL;
				LANSessionManager.StopLANSession();
			}
		}
	}
	else
	{
		if (LANSessionManager.GetBeaconState() != ELanBeaconState::Searching)
		{
			LANSessionManager.StopLANSession();
		}
	}
	return Result;
}

uint32 FOnlineSessionAccelByte::JoinLANSession(int32 PlayerNum, FNamedOnlineSession* Session,
	const FOnlineSession* SearchSession)
{
	uint32 Result = ONLINE_FAIL;
	Session->SessionState = EOnlineSessionState::Pending;

	if (Session->SessionInfo.IsValid() && SearchSession != nullptr && SearchSession->SessionInfo.IsValid())
	{
		const auto SearchSessionInfo = static_cast<const FOnlineSessionInfoAccelByte*>(SearchSession->SessionInfo.Get());
		auto SessionInfo = static_cast<FOnlineSessionInfoAccelByte*>(Session->SessionInfo.Get());
		SessionInfo->SessionId = SearchSessionInfo->SessionId;
		SessionInfo->HostAddr = SearchSessionInfo->HostAddr->Clone();
		Result = ONLINE_SUCCESS;
	}
	return Result;
}

uint32 FOnlineSessionAccelByte::FindLANSession()
{
	uint32 Return = ONLINE_IO_PENDING;
	GenerateNonce(reinterpret_cast<uint8*>(&LANSessionManager.LanNonce), 8);
	auto ResponseDelegate = FOnValidResponsePacketDelegate::CreateRaw(this, &FOnlineSessionAccelByte::OnValidResponsePacketReceived);
	auto TimeoutDelegate = FOnSearchingTimeoutDelegate::CreateRaw(this, &FOnlineSessionAccelByte::OnLANSearchTimeout);
	FNboSerializeToBufferAccelByte Packet(LAN_BEACON_MAX_PACKET_SIZE);
	LANSessionManager.CreateClientQueryPacket(Packet, LANSessionManager.LanNonce);
	if (!LANSessionManager.Search(Packet, ResponseDelegate, TimeoutDelegate))
	{
		Return = ONLINE_FAIL;
		FinalizeLANSearch();
		CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Failed;
		TriggerOnFindSessionsCompleteDelegates(false);
	}
	return Return;
}

uint32 FOnlineSessionAccelByte::FinalizeLANSearch()
{
	if (LANSessionManager.GetBeaconState() == ELanBeaconState::Searching)
	{
		LANSessionManager.StopLANSession();
	}
	return UpdateLANStatus();
}

void FOnlineSessionAccelByte::AppendSessionToPacket(FNboSerializeToBufferAccelByte& Packet, FOnlineSession* Session)
{
	Packet << *StaticCastSharedPtr<const FUniqueNetIdAccelByte>(Session->OwningUserId)
        << Session->OwningUserName
        << Session->NumOpenPrivateConnections
        << Session->NumOpenPublicConnections;
	SetPortFromNetDriver(*AccelByteSubsystem, Session->SessionInfo);
	Packet << *StaticCastSharedPtr<FOnlineSessionInfoAccelByte>(Session->SessionInfo);
	AppendSessionSettingsToPacket(Packet, &Session->SessionSettings);
}

void FOnlineSessionAccelByte::AppendSessionSettingsToPacket(FNboSerializeToBufferAccelByte& Packet,
	FOnlineSessionSettings* SessionSettings)
{
	Packet << SessionSettings->NumPublicConnections
        << SessionSettings->NumPrivateConnections
        << static_cast<uint8>(SessionSettings->bShouldAdvertise)
        << static_cast<uint8>(SessionSettings->bIsLANMatch)
        << static_cast<uint8>(SessionSettings->bIsDedicated)
        << static_cast<uint8>(SessionSettings->bUsesStats)
        << static_cast<uint8>(SessionSettings->bAllowJoinInProgress)
        << static_cast<uint8>(SessionSettings->bAllowInvites)
        << static_cast<uint8>(SessionSettings->bUsesPresence)
        << static_cast<uint8>(SessionSettings->bAllowJoinViaPresence)
        << static_cast<uint8>(SessionSettings->bAllowJoinViaPresenceFriendsOnly)
        << static_cast<uint8>(SessionSettings->bAntiCheatProtected)
        << SessionSettings->BuildUniqueId;

	int32 Num = 0;
	for (FSessionSettings::TConstIterator It(SessionSettings->Settings); It; ++It)
	{	
		const auto& Setting = It.Value();
		if (Setting.AdvertisementType >= EOnlineDataAdvertisementType::ViaOnlineService)
		{
			Num++;
		}
	}

	Packet << Num;
	for (FSessionSettings::TConstIterator It(SessionSettings->Settings); It; ++It)
	{
		const auto& Setting = It.Value();
		if (Setting.AdvertisementType >= EOnlineDataAdvertisementType::ViaOnlineService)
		{
			Packet << It.Key();
			Packet << Setting;
		}
	}
}

void FOnlineSessionAccelByte::ReadSessionFromPacket(FNboSerializeFromBufferAccelByte& Packet, FOnlineSession* Session)
{
	FUniqueNetIdAccelByte* UniqueId = new FUniqueNetIdAccelByte;
	Packet >> *UniqueId
        >> Session->OwningUserName
        >> Session->NumOpenPrivateConnections
        >> Session->NumOpenPublicConnections;

	Session->OwningUserId = MakeShareable(UniqueId);
	auto SessionInfo = new FOnlineSessionInfoAccelByte();
	SessionInfo->HostAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	Packet >> *SessionInfo;
	Session->SessionInfo = MakeShareable(SessionInfo); 

	ReadSettingsFromPacket(Packet, Session->SessionSettings);
}

void FOnlineSessionAccelByte::ReadSettingsFromPacket(FNboSerializeFromBufferAccelByte& Packet,
	FOnlineSessionSettings& SessionSettings)
{
    SessionSettings.Settings.Empty();
    Packet >> SessionSettings.NumPublicConnections
    	>> SessionSettings.NumPrivateConnections;
    uint8 Read = 0;
    Packet >> Read;
    SessionSettings.bShouldAdvertise = !!Read;
    Packet >> Read;
    SessionSettings.bIsLANMatch = !!Read;
    Packet >> Read;
    SessionSettings.bIsDedicated = !!Read;
    Packet >> Read;
    SessionSettings.bUsesStats = !!Read;
    Packet >> Read;
    SessionSettings.bAllowJoinInProgress = !!Read;
    Packet >> Read;
    SessionSettings.bAllowInvites = !!Read;
    Packet >> Read;
    SessionSettings.bUsesPresence = !!Read;
    Packet >> Read;
    SessionSettings.bAllowJoinViaPresence = !!Read;
    Packet >> Read;
    SessionSettings.bAllowJoinViaPresenceFriendsOnly = !!Read;
    Packet >> Read;
    SessionSettings.bAntiCheatProtected = !!Read;
    Packet >> SessionSettings.BuildUniqueId;
    int32 Num = 0;
    Packet >> Num;
    if (!Packet.HasOverflow())
    {
    	FName Key;
    	for (int32 i = 0;
    		i < Num && !Packet.HasOverflow();
    		i++)
    	{
    		FOnlineSessionSetting Setting;
    		Packet >> Key;
    		Packet >> Setting;
    		SessionSettings.Set(Key, Setting);
    	}
    }
    
    if (Packet.HasOverflow())
    {
    	SessionSettings.Settings.Empty();
    	UE_LOG_AB(Verbose, TEXT("Packet overflow detected"));
    }
}

void FOnlineSessionAccelByte::OnValidQueryPacketReceived(uint8* PacketData, int32 PacketLength, uint64 ClientNonce)
{
	FScopeLock ScopeLock(&SessionLock);
	for (int32 i = 0; i < Sessions.Num(); i++)
	{
		auto Session = &Sessions[i];
		if (Session && IsSessionJoinable(*Session))
		{
			FNboSerializeToBufferAccelByte Packet(LAN_BEACON_MAX_PACKET_SIZE);
			LANSessionManager.CreateHostResponsePacket(Packet, ClientNonce);
			AppendSessionToPacket(Packet, Session);
			if (!Packet.HasOverflow())
			{
				LANSessionManager.BroadcastPacket(Packet, Packet.GetByteCount());
			}
			else
			{
				UE_LOG_AB(Warning, TEXT("LAN broadcast packet overflow, cannot broadcast on LAN"));
			}
		}
	}
}

void FOnlineSessionAccelByte::OnValidResponsePacketReceived(uint8* PacketData, int32 PacketLength)
{
	FOnlineSessionSettings NewServer;
	if (CurrentSessionSearch.IsValid())
	{
		auto NewResult = new (CurrentSessionSearch->SearchResults) FOnlineSessionSearchResult();
		NewResult->PingInMs = static_cast<int32>((FPlatformTime::Seconds() - SessionSearchStartInSeconds) * 1000);
		FOnlineSession* NewSession = &NewResult->Session;
		FNboSerializeFromBufferAccelByte Packet(PacketData, PacketLength);		
		ReadSessionFromPacket(Packet, NewSession);
	}
	else
	{
		UE_LOG_AB(Warning, TEXT("Failed to create new online game setting"));
	}
}

void FOnlineSessionAccelByte::OnLANSearchTimeout()
{
	FinalizeLANSearch();
	if (CurrentSessionSearch.IsValid())
	{
		CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Done;
		CurrentSessionSearch = nullptr;
	}
	TriggerOnFindSessionsCompleteDelegates(true);
}

bool FOnlineSessionAccelByte::IsHost(const FNamedOnlineSession& Session) const
{
	if (AccelByteSubsystem->IsDedicated())
	{
		return true;
	}

	auto Identity = AccelByteSubsystem->GetIdentityInterface(); 
	if (!Identity.IsValid())
	{
		return false;
	}

	auto UserId = Identity->GetUniquePlayerId(Session.HostingPlayerNum);
	return (UserId.IsValid() && (*UserId == *Session.OwningUserId));
}

void FOnlineSessionAccelByte::SetPortFromNetDriver(const FOnlineSubsystemAccelByte& Subsystem,
	const TSharedPtr<FOnlineSessionInfo>& SessionInfo)
{
	auto Port = GetPortFromNetDriver(Subsystem.GetInstanceName());
	auto SessionInfoAccelByte = StaticCastSharedPtr<FOnlineSessionInfoAccelByte>(SessionInfo);
	if (SessionInfoAccelByte.IsValid() && SessionInfoAccelByte->HostAddr.IsValid())
	{
		//TODO: CHECK THIS
		SessionInfoAccelByte->HostAddr->SetPort(Port);
	}
}
