// Copyright (c) 2021 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#include "OnlineIdentityAccelByte.h"
#include "OnlineSubsystemAccelByte.h"
#include "OnlineError.h"
#include "OnlineSubsystemAccelByteLog.h"
#if USE_SIMPLE_SIGNALING
#include "Networking/AccelByteWebSocket.h"
#endif

bool FUserOnlineAccountAccelByte::GetAuthAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	const FString* FoundAttr = AdditionalAuthData.Find(AttrName);
	if (FoundAttr != nullptr)
	{
		OutAttrValue = *FoundAttr;
		return true;
	}
	return false;
}

FString FUserOnlineAccountAccelByte::GetRealName() const
{
	return "DummyName";
}

FString FUserOnlineAccountAccelByte::GetDisplayName(const FString& /*Platform*/) const
{
	return "DummyName";
}

bool FUserOnlineAccountAccelByte::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	const FString* FoundAttr = UserAttributes.Find(AttrName);
	if (FoundAttr != nullptr)
	{
		OutAttrValue = *FoundAttr;
		return true;
	}
	return false;
}

bool FUserOnlineAccountAccelByte::SetUserAttribute(const FString& AttrName, const FString& AttrValue)
{
	const FString* FoundAttr = UserAttributes.Find(AttrName);
	if (FoundAttr == nullptr || *FoundAttr != AttrValue)
	{
		UserAttributes.Add(AttrName, AttrValue);
		return true;
	}
	return false;
}

bool FOnlineIdentityAccelByte::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	if (LocalUserNum < 0 || LocalUserNum >= MAX_LOCAL_PLAYERS)
	{
		TriggerOnLoginChangedDelegates(LocalUserNum);
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, FUniqueNetIdAccelByte(),
			FString::Printf(TEXT("Invalid LocalUserNum=%d"), LocalUserNum));
		return false;
	}
#if USE_SIMPLE_SIGNALING
	AccelByteWebSocket::Instance()->OnWebSocketConnectedDelegate.BindRaw(this, &FOnlineIdentityAccelByte::LoginSuccess);
	AccelByteWebSocket::Instance()->Connect();
#endif
	return true;
}

bool FOnlineIdentityAccelByte::Logout(int32 LocalUserNum)
{
	//FRegistry::User.ForgetAllCredentials();
	TriggerOnLogoutCompleteDelegates(LocalUserNum, true);
	return true;
}

bool FOnlineIdentityAccelByte::AutoLogin(int32 LocalUserNum)
{
	//Not implemented, no auto login at this moment
	//TODO: auto login from command line
	TriggerOnLoginCompleteDelegates(0, false, FUniqueNetIdAccelByte(), TEXT("AutoLogin failed. Not implemented."));
	return false;
}

TSharedPtr<FUserOnlineAccount> FOnlineIdentityAccelByte::GetUserAccount(const FUniqueNetId& UserId) const
{
	return CurrentUserAccount;
}

TArray<TSharedPtr<FUserOnlineAccount> > FOnlineIdentityAccelByte::GetAllUserAccounts() const
{
	TArray<TSharedPtr<FUserOnlineAccount> > Result;
	Result.Add(CurrentUserAccount);
	return Result;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityAccelByte::GetUniquePlayerId(int32 LocalUserNum) const
{
	return CurrentUser;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityAccelByte::CreateUniquePlayerId(uint8* Bytes, int32 Size)
{
	if (Bytes != nullptr && Size > 0)
	{
		FString StrId(Size, (TCHAR*)Bytes);
		return MakeShareable(new FUniqueNetIdAccelByte(StrId));
	}
	return nullptr;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityAccelByte::CreateUniquePlayerId(const FString& Str)
{
	return MakeShareable(new FUniqueNetIdAccelByte(Str));
}

ELoginStatus::Type FOnlineIdentityAccelByte::GetLoginStatus(int32 LocalUserNum) const
{
	if(CurrentUser.IsValid())
	{
		return ELoginStatus::LoggedIn;
	}
	return ELoginStatus::NotLoggedIn;
}

ELoginStatus::Type FOnlineIdentityAccelByte::GetLoginStatus(const FUniqueNetId& UserId) const
{	
	if(CurrentUser.IsValid())
	{
		return ELoginStatus::LoggedIn;
	}
	return ELoginStatus::NotLoggedIn;
}

FString FOnlineIdentityAccelByte::GetPlayerNickname(int32 LocalUserNum) const
{
	TSharedPtr<const FUniqueNetId> UniqueId = GetUniquePlayerId(LocalUserNum);
	if (UniqueId.IsValid())
	{
		return UniqueId->ToString();
	}

	return TEXT("User-Not-Valid");
}

FString FOnlineIdentityAccelByte::GetPlayerNickname(const FUniqueNetId& UserId) const
{
	return UserId.ToString();
}

FString FOnlineIdentityAccelByte::GetAuthToken(int32 LocalUserNum) const
{
	TSharedPtr<const FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
	if (UserId.IsValid())
	{
		TSharedPtr<FUserOnlineAccount> UserAccount = GetUserAccount(*UserId);
		if (UserAccount.IsValid())
		{
			return UserAccount->GetAccessToken();
		}
	}
	return FString();
}

void FOnlineIdentityAccelByte::RevokeAuthToken(const FUniqueNetId& UserId, const FOnRevokeAuthTokenCompleteDelegate& Delegate)
{
	UE_LOG_ONLINE_IDENTITY(Display, TEXT("FOnlineIdentityAccelByte::RevokeAuthToken not implemented"));
	TSharedRef<const FUniqueNetId> UserIdRef(UserId.AsShared());
	AccelByteSubsystem->ExecuteNextTick([UserIdRef, Delegate]()
	{
		Delegate.ExecuteIfBound(*UserIdRef, FOnlineError(FString(TEXT("RevokeAuthToken not implemented"))));
	});
}

FOnlineIdentityAccelByte::FOnlineIdentityAccelByte(FOnlineSubsystemAccelByte* InSubsystem)
	: AccelByteSubsystem(InSubsystem)
{
}

FOnlineIdentityAccelByte::~FOnlineIdentityAccelByte()
{
}

void FOnlineIdentityAccelByte::LoginSuccess(const FString &Id)
{
	FString ErrorStr;
	
	if (Id.IsEmpty()) 
	{
		ErrorStr = TEXT("User is not logged in");
	}
	else {
		CurrentUserAccount = MakeShared<FUserOnlineAccountAccelByte>(Id);
		CurrentUser = CurrentUserAccount->GetUserId();
	}
	
	TriggerOnLoginChangedDelegates(0);
	TriggerOnLoginCompleteDelegates(0, true, FUniqueNetIdAccelByte(Id), ErrorStr);
}

void FOnlineIdentityAccelByte::LoginFailed(int32 ErrorCode, const FString& ErrorMessage)
{
	UE_LOG_AB(Log, TEXT("LOGIN FAILED WITH CODE : %d AND ERROR MESSAGE : %s"), ErrorCode, *ErrorMessage);
	TriggerOnLoginChangedDelegates(0);
	TriggerOnLoginCompleteDelegates(0, true, FUniqueNetIdAccelByte(), ErrorMessage);
}

void FOnlineIdentityAccelByte::GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate)
{
	//Not implemented, allow all access for now
	Delegate.ExecuteIfBound(UserId, Privilege, (uint32)EPrivilegeResults::NoFailures);
}

FPlatformUserId FOnlineIdentityAccelByte::GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId) const
{
	if (CurrentUser.IsValid()) {
		return 0;
	}
	return PLATFORMUSERID_NONE;
}

FString FOnlineIdentityAccelByte::GetAuthType() const
{
	return TEXT("AccelByte");
}