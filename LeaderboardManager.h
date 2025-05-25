#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "OnlineError.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineLeaderboardInterface.h"
#include "Interfaces/OnlineStatsInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "LeaderboardManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLeaderBoardFlushCompleted, FName, SessionName, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLeaderBoardQueryCompleted, FName, SessionName, bool, bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLeaderboardWindowShow, bool);

UENUM(BlueprintType)
enum class ELeaderboardPlatform : uint8
{
    Steam,
    Epic
};

USTRUCT(BlueprintType)
struct FLeaderboardPlatformMappingRow : public FTableRowBase
{
    GENERATED_BODY();

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString LeaderboardDisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SteamLeaderboardName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SteamStatName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EpicLeaderboardName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EpicStatName;
};

USTRUCT(BlueprintType)
struct FLeaderboardEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PlayerName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Score;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Rank;

    FLeaderboardEntry()
        : PlayerName(TEXT("Unknown")), Score(0), Rank(0) {}
};

UCLASS()
class YOUR_GAME_API ULeaderboardManager : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(UDataTable* InTable);

    UFUNCTION(BlueprintCallable, Category = "Leaderboard")
    void WriteToLeaderboard(const FString& WorldName, const FString& LeaderboardName, int32 Score);
    
    UFUNCTION(BlueprintCallable, Category = "Leaderboard")
    void ReadLeaderboard(const FString& WorldName, const FString& LeaderboardName,  bool bFriendsOnly, int32 RankFirst, int32 RankCount, bool DoNotShowWindow = false);

    UFUNCTION(BlueprintCallable, Category = "Leaderboard")
    const TArray<FLeaderboardEntry>& GetLeaderboardByName(const FString& LeaderboardName) const;

    UFUNCTION(BlueprintCallable, Category = "Leaderboard")
    void GetMappedLeaderboardAndStat(const FString& DisplayName, FString& OutLeaderboardName, FString& OutStatName);

    const TMap<FString, TArray<FLeaderboardEntry>>& GetLeaderboardEntries() const;

    FOnLeaderboardWindowShow OnLeaderboardWindowShow;

    UPROPERTY(BlueprintAssignable, Category = "Leaderboard")
    FOnLeaderBoardFlushCompleted OnLeaderBoardFlushCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Leaderboard")
    FOnLeaderBoardQueryCompleted OnLeaderBoardQueryCompleted;

private:
    void WriteToSteamLeaderboard(const FString& WorldName, const FString& LeaderboardName, int32 Score);
    void WriteToEpicLeaderboard(const FString& WorldName, const FString& LeaderboardName, int32 Score);
    void ReadFromSteamLeaderboard(const FString& WorldName, const FString& LeaderboardName, bool bFriendsOnly, int32 RankFirst, int32 RankCount, bool DoNotShowWindow = false);
    void ReadFromEpicLeaderboard(const FString& WorldName, const FString& LeaderboardName, bool bFriendsOnly, int32 RankFirst, int32 RankCount, bool DoNotShowWindow = false);

    void OnLeaderboardReadComplete(bool bWasSuccessful, FOnlineLeaderboardReadRef LeaderboardReadRef);
    void OnLeaderboardFlushComplete(FName SessionName, bool bWasSuccessful);

    FDelegateHandle QueryLeaderboardDelegateHandle;
    FDelegateHandle FlushLeaderboardDelegateHandle;

    TMap<FString, TArray<FLeaderboardEntry>> LeaderboardEntries;

    UPROPERTY()
    UDataTable* LeaderboardMappingTable;

    bool bIsFriendLeaderboard = false;
    bool bIsDoNotShowWindow = false;
    ELeaderboardPlatform PlatformType = ELeaderboardPlatform::Steam;
};
