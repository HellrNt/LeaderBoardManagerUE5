#include "LeaderboardManager.h"
#include "OnlineSubsystem.h"
#include "OnlineStats.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

void ULeaderboardManager::Initialize(UDataTable* InTable)
{
    LeaderboardMappingTable = InTable;
    LeaderboardEntries.Empty();

    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
    if (Subsystem)
    {
        FString SubsystemName = Subsystem->GetSubsystemName().ToString();
        if (SubsystemName == "EOS" || SubsystemName == "Epic")
        {
            PlatformType = ELeaderboardPlatform::Epic;
        }
        else
        {
            PlatformType = ELeaderboardPlatform::Steam;
        }
    }
}

void ULeaderboardManager::WriteToLeaderboard(const FString& WorldName, const FString& LeaderboardName, int32 Score)
{
    switch (PlatformType)
    {
        case ELeaderboardPlatform::Steam:
        {
            WriteToSteamLeaderboard(WorldName, LeaderboardName, Score);
            break;
        }
        case ELeaderboardPlatform::Epic:
        {
            WriteToEpicLeaderboard(WorldName, LeaderboardName, Score);
            break;
        }
    }
}

void ULeaderboardManager::ReadLeaderboard(const FString& WorldName, const FString& LeaderboardName, bool bFriendsOnly, int32 RankFirst, int32 RankCount, bool DoNotShowWindow)
{
    switch (PlatformType)
    {
        case ELeaderboardPlatform::Steam:
        {
            ReadFromSteamLeaderboard(WorldName, LeaderboardName, bFriendsOnly, RankFirst, RankCount, DoNotShowWindow);
            break;
        }
        case ELeaderboardPlatform::Epic:
        {
            ReadFromEpicLeaderboard(WorldName, LeaderboardName, bFriendsOnly, RankFirst, RankCount, DoNotShowWindow);
            break;
        }
    }
}

const TMap<FString, TArray<FLeaderboardEntry>>& ULeaderboardManager::GetLeaderboardEntries() const
{
    return LeaderboardEntries;
}

const TArray<FLeaderboardEntry>& ULeaderboardManager::GetLeaderboardByName(const FString& LeaderboardName) const
{
    if (const TArray<FLeaderboardEntry>* Found = LeaderboardEntries.Find(LeaderboardName))
    {
        return *Found;
    }

    static const TArray<FLeaderboardEntry> EmptyArray;
    return EmptyArray;
}

void ULeaderboardManager::OnLeaderboardReadComplete(bool bWasSuccessful, FOnlineLeaderboardReadRef LeaderboardReadRef)
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
    if (Subsystem)
    {
        IOnlineLeaderboardsPtr Leaderboards = Subsystem->GetLeaderboardsInterface();
        if (bWasSuccessful)
        {
            if (LeaderboardEntries.Contains(LeaderboardReadRef->LeaderboardName.ToString()))
            {
                LeaderboardEntries[LeaderboardReadRef->LeaderboardName.ToString()].Empty();
            }
            int32 MyRank = -1;
            TSharedPtr<const FUniqueNetId> UserId;
            IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
            if (Identity.IsValid())
            {
                UserId = Identity->GetUniquePlayerId(0);
            }
            UE_LOG(LogTemp, Log, TEXT("Leaderboard data successfully read."));

            for (const FOnlineStatsRow& Row : LeaderboardReadRef->Rows)
            {
                FString LocalPlayerName = Row.NickName;
                int32 PlayerScore = 0;

                const FVariantData* ScoreData = Row.Columns.Find(LeaderboardReadRef->SortedColumn);
                if (ScoreData)
                {
                    ScoreData->GetValue(PlayerScore);
                }
                FLeaderboardEntry NewEntry;
                NewEntry.PlayerName = LocalPlayerName;
                NewEntry.Score = PlayerScore;
                NewEntry.Rank = Row.Rank;
                LeaderboardEntries.FindOrAdd(LeaderboardReadRef->LeaderboardName.ToString()).Add(NewEntry);
                if (MyRank == -1 && UserId.IsValid() && Row.PlayerId == UserId)
                {
                    MyRank = Row.Rank;
                }
                UE_LOG(LogTemp, Log, TEXT("Player: %s, Score: %d"), *LocalPlayerName, PlayerScore);
            }

            if (LeaderboardEntries.Contains(LeaderboardReadRef->LeaderboardName.ToString()) && LeaderboardEntries[LeaderboardReadRef->LeaderboardName.ToString()].Num() > 1)
            {
                LeaderboardEntries[LeaderboardReadRef->LeaderboardName.ToString()].Sort([](const FLeaderboardEntry& A, const FLeaderboardEntry& B)
                {
                    return A.Rank < B.Rank;
                });
            }
            
            if (!bIsDoNotShowWindow)
            {
                OnLeaderboardWindowShow.Broadcast(bIsFriendLeaderboard);
            }

            OnLeaderBoardQueryCompleted.Broadcast(NAME_None, true);
        }
        else
        {
            if (LeaderboardEntries.Contains(LeaderboardReadRef->LeaderboardName.ToString()))
            {
                LeaderboardEntries[LeaderboardReadRef->LeaderboardName.ToString()].Empty();
            }
            if (!bIsDoNotShowWindow)
            {
                OnLeaderboardWindowShow.Broadcast(bIsFriendLeaderboard);
            }
            UE_LOG(LogTemp, Warning, TEXT("Failed to read leaderboard data."));
            OnLeaderBoardQueryCompleted.Broadcast(NAME_None, false);
        }
        if (Leaderboards)
        {
            Leaderboards->ClearOnLeaderboardReadCompleteDelegate_Handle(QueryLeaderboardDelegateHandle);
        }
    }
}

void ULeaderboardManager::OnLeaderboardFlushComplete(FName SessionName, bool bWasSuccessful)
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
    if (Subsystem)
    {
        IOnlineLeaderboardsPtr Leaderboards = Subsystem->GetLeaderboardsInterface();
        if (Leaderboards.IsValid())
        {
            Leaderboards->ClearOnLeaderboardFlushCompleteDelegate_Handle(FlushLeaderboardDelegateHandle);
            FlushLeaderboardDelegateHandle.Reset();
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Leaderboard flush %s for session: %s"),
        bWasSuccessful ? TEXT("succeeded") : TEXT("failed"),
        *SessionName.ToString());

    OnLeaderBoardFlushCompleted.Broadcast(SessionName, bWasSuccessful);
}

void ULeaderboardManager::GetMappedLeaderboardAndStat(const FString& DisplayName, FString& OutLeaderboardName, FString& OutStatName)
{
    if (!LeaderboardMappingTable) return;

    const FLeaderboardPlatformMappingRow* Mapping = LeaderboardMappingTable->FindRow<FLeaderboardPlatformMappingRow>(*DisplayName, TEXT(""));
    if (!Mapping) return;

    switch (PlatformType)
    {
        case ELeaderboardPlatform::Steam:
        {
            OutLeaderboardName = Mapping->SteamLeaderboardName;
            OutStatName = Mapping->SteamStatName;
            break;

        }
        case ELeaderboardPlatform::Epic:
        {
            OutLeaderboardName = Mapping->EpicLeaderboardName;
            OutStatName = Mapping->EpicStatName;
            break;
        }
    }
}

// ===== Steam stuff =====

void ULeaderboardManager::WriteToSteamLeaderboard(const FString& WorldName, const FString& LeaderboardName, int32 Score)
{
    IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
    if (OnlineSub)
    {
        IOnlineIdentityPtr IdentityInterface = OnlineSub->GetIdentityInterface();
        if (IdentityInterface.IsValid())
        {
            FUniqueNetIdPtr UserId = IdentityInterface->GetUniquePlayerId(0);
            if (UserId.IsValid())
            {
                IOnlineLeaderboardsPtr Leaderboards = OnlineSub->GetLeaderboardsInterface();
                if (Leaderboards.IsValid())
                {
                    FString LeaderboardAPIName, StatName;
                    GetMappedLeaderboardAndStat(LeaderboardName, LeaderboardAPIName, StatName);
                    FOnlineLeaderboardWrite WriteObject;
                    WriteObject.LeaderboardNames.Add(FName(*LeaderboardName));
                    WriteObject.RatedStat = FName(*LeaderboardAPIName);
                    WriteObject.SortMethod = ELeaderboardSort::Descending;
                    WriteObject.UpdateMethod = ELeaderboardUpdateMethod::KeepBest;
                    WriteObject.SetIntStat(FName(*StatName), Score);

                    if (Leaderboards->WriteLeaderboards(FName(WorldName), *UserId, WriteObject))
                    {
                        FlushLeaderboardDelegateHandle = Leaderboards->AddOnLeaderboardFlushCompleteDelegate_Handle(
                            FOnLeaderboardFlushCompleteDelegate::CreateUObject(this, &ULeaderboardManager::OnLeaderboardFlushComplete));
                        Leaderboards->FlushLeaderboards(FName(WorldName));
                    }
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Failed to get UserId."));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("IdentityInterface is not valid."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("OnlineSubsystem is not available."));
    }
}

void ULeaderboardManager::ReadFromSteamLeaderboard(const FString& WorldName, const FString& LeaderboardName, bool bFriendsOnly, int32 RankFirst, int32 RankCount, bool DoNotShowWindow)
{
    if (IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
    {
        IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
        if (Identity.IsValid())
        {
            FUniqueNetIdPtr NetId = Identity->GetUniquePlayerId(0);
            if (!NetId)
            {
                return;
            }
        }

        IOnlineLeaderboardsPtr Leaderboards = Subsystem->GetLeaderboardsInterface();
        if (Leaderboards.IsValid())
        {
            FString LeaderboardAPIName, StatName;
            GetMappedLeaderboardAndStat(LeaderboardName, LeaderboardAPIName, StatName);
            bIsDoNotShowWindow = DoNotShowWindow;
            FOnlineLeaderboardReadRef LeaderboardReadRef = MakeShared<FOnlineLeaderboardRead>();
            LeaderboardReadRef->LeaderboardName = FName(*LeaderboardAPIName);
            LeaderboardReadRef->SortedColumn = FName(*StatName);
            FColumnMetaData ColumnMetaData = FColumnMetaData(FName(*StatName), EOnlineKeyValuePairDataType::Int32);
            LeaderboardReadRef->ColumnMetadata.Add(ColumnMetaData);
            LeaderboardReadRef->Rows.Empty();

            QueryLeaderboardDelegateHandle =
                Leaderboards->AddOnLeaderboardReadCompleteDelegate_Handle(FOnLeaderboardReadCompleteDelegate::CreateUObject(
                    this,
                    &ULeaderboardManager::OnLeaderboardReadComplete, LeaderboardReadRef));

            bIsFriendLeaderboard = bFriendsOnly;
            if (bFriendsOnly)
            {
                if (!Leaderboards->ReadLeaderboardsForFriends(0, LeaderboardReadRef))
                {
                    UE_LOG(LogTemp, Error, TEXT("Failed to read friends leaderboard."));
                    Leaderboards->ClearOnLeaderboardReadCompleteDelegate_Handle(QueryLeaderboardDelegateHandle);
                    QueryLeaderboardDelegateHandle.Reset();
                }
            }
            else
            {
                if (!Leaderboards->ReadLeaderboardsAroundRank(RankFirst, RankCount, LeaderboardReadRef))
                {
                    UE_LOG(LogTemp, Error, TEXT("Failed to read global leaderboard."));
                    Leaderboards->ClearOnLeaderboardReadCompleteDelegate_Handle(QueryLeaderboardDelegateHandle);
                    QueryLeaderboardDelegateHandle.Reset();
                }
            }
        }
    }
}

// ===== Epic stuff =====

void ULeaderboardManager::WriteToEpicLeaderboard(const FString& WorldName, const FString& LeaderboardName, int32 Score)
{
    if (IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
    {
        IOnlineStatsPtr StatsInterface = Subsystem->GetStatsInterface();
        if (StatsInterface.IsValid())
        {
            IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
            if (Identity.IsValid())
            {
                FUniqueNetIdPtr UserId = Identity->GetUniquePlayerId(0);
                if (UserId.IsValid())
                {
                    IOnlineLeaderboardsPtr Leaderboards = Subsystem->GetLeaderboardsInterface();
                    if (Leaderboards.IsValid())
                    {
                        FString LeaderboardAPIName, StatName;
                        GetMappedLeaderboardAndStat(LeaderboardName, LeaderboardAPIName, StatName);
                        FOnlineLeaderboardWrite WriteObject;
                        WriteObject.LeaderboardNames.Add(FName(*LeaderboardAPIName));
                        WriteObject.RatedStat = FName(*StatName);
                        WriteObject.SortMethod = ELeaderboardSort::Descending;
                        WriteObject.UpdateMethod = ELeaderboardUpdateMethod::KeepBest;
                        WriteObject.SetIntStat(FName(*StatName), Score);

                        if (Leaderboards->WriteLeaderboards(FName(WorldName), *UserId, WriteObject))
                        {
                            FlushLeaderboardDelegateHandle = Leaderboards->AddOnLeaderboardFlushCompleteDelegate_Handle(
                                FOnLeaderboardFlushCompleteDelegate::CreateUObject(this, &ULeaderboardManager::OnLeaderboardFlushComplete));
                            Leaderboards->FlushLeaderboards(FName(WorldName));
                        }
                    }
                }
            }
        }
    }
}

void ULeaderboardManager::ReadFromEpicLeaderboard(const FString& WorldName, const FString& LeaderboardName, bool bFriendsOnly, int32 RankFirst, int32 RankCount, bool DoNotShowWindow)
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
    if (Subsystem)
    {
        IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
        if (Identity.IsValid())
        {
            FUniqueNetIdPtr NetId = Identity->GetUniquePlayerId(0);
            if (!NetId || Identity->GetLoginStatus(*NetId) != ELoginStatus::LoggedIn)
            {
                return;
            }
        }

        IOnlineLeaderboardsPtr Leaderboards = Subsystem->GetLeaderboardsInterface();
        if (Leaderboards.IsValid())
        {
            FString LeaderboardAPIName, StatName;
            GetMappedLeaderboardAndStat(LeaderboardName, LeaderboardAPIName, StatName);
            bIsDoNotShowWindow = DoNotShowWindow;
            FOnlineLeaderboardReadRef LeaderboardReadRef = MakeShared<FOnlineLeaderboardRead, ESPMode::ThreadSafe>();
            LeaderboardReadRef->LeaderboardName = FName(*LeaderboardAPIName);
            LeaderboardReadRef->SortedColumn = FName(*StatName);
            FColumnMetaData ColumnMetaData = FColumnMetaData(FName(*StatName), EOnlineKeyValuePairDataType::Int32);
            LeaderboardReadRef->ColumnMetadata.Add(ColumnMetaData);
            QueryLeaderboardDelegateHandle =
                Leaderboards->AddOnLeaderboardReadCompleteDelegate_Handle(FOnLeaderboardReadCompleteDelegate::CreateUObject(
                    this,
                    &ULeaderboardManager::OnLeaderboardReadComplete, LeaderboardReadRef));

            bIsFriendLeaderboard = bFriendsOnly;
            if (bFriendsOnly)
            {
                if (!Leaderboards->ReadLeaderboardsForFriends(0, LeaderboardReadRef))
                {
                    UE_LOG(LogTemp, Error, TEXT("Failed to read friends leaderboard."));
                    Leaderboards->ClearOnLeaderboardReadCompleteDelegate_Handle(QueryLeaderboardDelegateHandle);
                    QueryLeaderboardDelegateHandle.Reset();
                }
            }
            else
            {
                if (!Leaderboards->ReadLeaderboardsAroundRank(RankFirst, RankCount, LeaderboardReadRef))
                {
                    UE_LOG(LogTemp, Error, TEXT("Failed to read global leaderboard."));
                    Leaderboards->ClearOnLeaderboardReadCompleteDelegate_Handle(QueryLeaderboardDelegateHandle);
                    QueryLeaderboardDelegateHandle.Reset();
                }
            }
        }
    }
}
