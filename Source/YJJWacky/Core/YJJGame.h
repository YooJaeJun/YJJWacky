#pragma once

#include "CoreMinimal.h"
#include "Physics/YJJWobbler.h"
#include "Subsystems/WorldSubsystem.h"
#include "YJJGame.generated.h"

class AYJJArena;
class AYJJTower;

UENUM()
enum class EYJJTowerType : uint8
{
	Punch,		// 권투 장갑: 길 밖으로 날려 버린다
	Cannon,		// 대포: 멀리서 포탄을 굴린다
	Fan,		// 선풍기: 바람으로 밀어낸다
	Guard,		// 몽둥이 병사: 길목에서 빙글빙글
	Sling,		// 새총: 가벼운 놈을 붙잡아 탄알로 쏜다
};

inline constexpr int32 YJJTowerTypeCount = 5;
inline constexpr int32 YJJMaxTowerLevel = 3;

struct FYJJTowerInfo
{
	const TCHAR* Name;
	const TCHAR* Blurb;
	int32 Cost;
	float Range;
	/** 강화 단계별 한 줄 설명 (Lv2, Lv3) */
	const TCHAR* Upgrade2;
	const TCHAR* Upgrade3;
};

/** 웨이브 한 줄: 총 무게 예산 중 이 몬스터 몫, 나오는 구간(0~1) */
struct FYJJWaveShare
{
	EYJJWobblerKind Kind;
	float Share;
	float From = 0.f;
	float To = 1.f;
};

struct FYJJWaveDef
{
	int32 BudgetKg;
	float Duration;
	TArray<FYJJWaveShare> Shares;
};

enum class EYJJPhase : uint8
{
	Build,	// 웨이브 사이: 타워를 짓고 출격 버튼을 기다린다
	Wave,
	Won,
	Lost,
};

/** 월드 위에 뜨는 글자 (+8, 풍덩!) */
struct FYJJFloater
{
	FVector Location = FVector::ZeroVector;
	FString Text;
	FLinearColor Color = FLinearColor::White;
	float Size = 26.f;
	float Age = 0.f;
};

/** 게임 규칙: 골드, 푸딩 체력, 웨이브, 타워 짓기, 떼죽음 연출 */
UCLASS()
class YJJWACKY_API UYJJGame : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 WaveCount = 10;
	static constexpr int32 MaxLives = 20;

	static UYJJGame* Get(const UObject* WorldContext);
	static const FYJJTowerInfo& TowerInfo(EYJJTowerType Type);
	/** Level 단계에서 다음 단계로 가는 비용 */
	static int32 UpgradeCost(EYJJTowerType Type, int32 Level);
	/** 새총이 Level 단계에서 들 수 있는 최대 무게 */
	static float SlingLimitKg(int32 Level);
	static const FYJJWaveDef& WaveDef(int32 WaveNumber);
	/** 웨이브 구성: 몬스터별 마릿수 (무게 예산 / 몬스터 무게) */
	static TArray<TPair<EYJJWobblerKind, int32>> WaveCounts(int32 WaveNumber);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	EYJJPhase GetPhase() const { return Phase; }
	int32 GetGold() const { return Gold; }
	int32 GetLives() const { return Lives; }
	/** 지금 진행 중이거나 방금 끝낸 웨이브 (1부터). 아직 시작 전이면 0 */
	int32 GetWave() const { return Wave; }
	int32 GetEnemiesLeft() const { return Enemies.Num() + SpawnQueue.Num() - SpawnIndex; }
	int32 GetKills() const { return Kills; }
	int32 GetDrowned() const { return Drowned; }
	int32 GetLeaked() const { return Leaked; }
	int32 GetBestCombo() const { return BestCombo; }
	FString GetWavePreview(int32 WaveNumber) const;

	// 타워
	bool HasTower(int32 Pad) const;
	EYJJTowerType GetPadTower(int32 Pad) const;
	int32 GetPadLevel(int32 Pad) const;
	AYJJTower* GetTower(int32 Pad) const { return HasTower(Pad) ? Towers[Pad].Get() : nullptr; }
	int32 SellValue(int32 Pad) const;
	bool Build(int32 Pad, EYJJTowerType Type);
	bool Upgrade(int32 Pad);
	bool Sell(int32 Pad);

	void StartWave();

	/** 딱밤: 적 하나를 손가락으로 튕긴다 */
	bool Flick(AYJJWobbler* Enemy, const FVector& Direction);
	float GetFlickCharge() const { return 1.f - FMath::Clamp(FlickCooldown / FlickDelay, 0.f, 1.f); }

	// 흐느적이
	AYJJWobbler* SpawnEnemy(EYJJWobblerKind Kind);
	void RegisterWobbler(AYJJWobbler* Wobbler);
	void OnWobblerDown(AYJJWobbler* Wobbler, bool bDrowned);
	void OnEnemyReachedBase(AYJJWobbler* Wobbler);
	/** 살아 있는 적 */
	const TArray<TObjectPtr<AYJJWobbler>>& GetEnemies() const { return Enemies; }
	const TArray<TObjectPtr<AYJJWobbler>>& GetGuards() const { return Guards; }
	AYJJWobbler* FindNearestEnemy(const FVector& Location, float Radius) const;

	// HUD
	void AddFloater(const FVector& Location, const FString& Text, const FLinearColor& Color, float Size = 26.f);
	const TArray<FYJJFloater>& GetFloaters() const { return Floaters; }
	void ShowBanner(const FString& Text, const FLinearColor& Color, float Life = 2.2f);
	const FString& GetBannerText() const { return BannerText; }
	FLinearColor GetBannerColor() const { return BannerColor; }
	float GetBannerAge() const { return BannerAge; }
	float GetBannerLife() const { return BannerLife; }
	/** 게임이 끝난 뒤 흐른 시간 (실시간 초) */
	float GetEndAge() const { return EndAge; }

	/** 테스트용: 골드를 정하고, bClear면 적과 타워를 모두 치운다 */
	void DebugReset(int32 InGold, bool bClear);

private:
	struct FSpawn
	{
		float Time;
		EYJJWobblerKind Kind;
	};

	AYJJArena* Arena();
	void SpawnGuard(int32 Pad, int32 Level);
	void EndGame(bool bWon);

	UPROPERTY()
	TArray<TObjectPtr<AYJJWobbler>> Enemies;

	/** 발판 번호 순. 안 지은 자리는 nullptr */
	UPROPERTY()
	TArray<TObjectPtr<AYJJTower>> Towers;

	UPROPERTY()
	TArray<TObjectPtr<AYJJWobbler>> Guards;

	UPROPERTY()
	TObjectPtr<AYJJArena> CachedArena;

	TArray<float> GuardRespawn;

	EYJJPhase Phase = EYJJPhase::Build;
	int32 Gold = 240;
	int32 Lives = MaxLives;
	int32 Wave = 0;
	int32 Kills = 0;
	int32 Drowned = 0;
	int32 Leaked = 0;
	int32 BestCombo = 0;

	TArray<FSpawn> SpawnQueue;
	int32 SpawnIndex = 0;
	float WaveTime = 0.f;

	static constexpr float FlickDelay = 1.2f;
	float FlickCooldown = 0.f;

	TArray<double> RecentDowns;
	double LastComboTime = -100.0;
	float SlowMoLeft = 0.f;

	TArray<FYJJFloater> Floaters;
	FString BannerText;
	FLinearColor BannerColor = FLinearColor::White;
	float BannerAge = 0.f;
	float BannerLife = 0.f;
	float EndAge = 0.f;
};
