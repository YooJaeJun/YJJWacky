#pragma once

#include "CoreMinimal.h"
#include "Data/YJJData.h"
#include "Physics/YJJWobbler.h"
#include "Subsystems/WorldSubsystem.h"
#include "YJJGame.generated.h"

class AYJJArena;
class AYJJTower;

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
	float Size = 0.f;
	float Age = 0.f;
};

/** 게임 규칙: 골드, 푸딩 체력, 웨이브, 타워 짓기, 떼죽음 연출 */
UCLASS()
class YJJWACKY_API UYJJGame : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UYJJGame* Get(const UObject* WorldContext);

	/** 수치는 Data/*.csv (YJJData), 글자는 Localization.csv (YJJLoc) */
	static int32 GetWaveCount() { return YJJData::WaveCount(); }
	static int32 GetMaxLives();
	static const FYJJTowerRow& TowerInfo(EYJJTowerType Type) { return YJJData::Tower(Type); }
	static FString TowerName(EYJJTowerType Type);
	static FString TowerBlurb(EYJJTowerType Type);
	/** Level 단계에서 다음 단계로 갈 때 얻는 것 (한 줄) */
	static FString UpgradeText(EYJJTowerType Type, int32 Level);
	/** Level 단계에서 다음 단계로 가는 비용 */
	static int32 UpgradeCost(EYJJTowerType Type, int32 Level);
	/** 새총이 Level 단계에서 들 수 있는 최대 무게 */
	static float SlingLimitKg(int32 Level);
	static const FYJJWaveDef& WaveDef(int32 WaveNumber) { return YJJData::Wave(WaveNumber); }
	/** 웨이브 구성: 몬스터별 마릿수 (무게 예산 / 몬스터 무게) */
	static TArray<TPair<EYJJWobblerKind, int32>> WaveCounts(int32 WaveNumber);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
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
	float GetFlickCharge() const;

	// 흐느적이
	AYJJWobbler* SpawnEnemy(EYJJWobblerKind Kind);
	void RegisterWobbler(AYJJWobbler* Wobbler);
	void OnWobblerDown(AYJJWobbler* Wobbler, bool bDrowned);
	void OnEnemyReachedBase(AYJJWobbler* Wobbler);
	/** 살아 있는 적 */
	const TArray<TObjectPtr<AYJJWobbler>>& GetEnemies() const { return Enemies; }
	const TArray<TObjectPtr<AYJJWobbler>>& GetGuards() const { return Guards; }
	AYJJWobbler* FindNearestEnemy(const FVector& Location, float Radius) const;

	// HUD. Style은 TextStyles.csv 의 행 이름, Scale은 크기 배율
	void AddFloater(const FVector& Location, const FString& Text, const TCHAR* Style, float Scale = 1.f);
	const TArray<FYJJFloater>& GetFloaters() const { return Floaters; }
	void ShowBanner(const FString& Text, const TCHAR* Style);
	const FString& GetBannerText() const { return BannerText; }
	FLinearColor GetBannerColor() const { return BannerColor; }
	float GetBannerAge() const { return BannerAge; }
	float GetBannerLife() const { return BannerLife; }
	float GetBannerSize() const { return BannerSize; }
	/** 게임이 끝난 뒤 흐른 시간 (실시간 초) */
	float GetEndAge() const { return EndAge; }

	/** 테스트용: 골드를 정하고, bClear면 적과 타워를 모두 치운다 */
	void DebugReset(int32 InGold, bool bClear);

	/** 사진 모드: HUD 를 그리지 않는다 (자동 테스트 gallery, 개발 영상용 장면) */
	void SetPhotoMode(bool bOn) { bPhotoMode = bOn; }
	bool IsPhotoMode() const { return bPhotoMode; }

private:
	bool bPhotoMode = false;

	struct FSpawn
	{
		float Time;
		EYJJWobblerKind Kind;
	};

	static float Tune(const TCHAR* Name) { return YJJData::Tune(Name); }

	// 적이 나오는 자리 고르기 (모양에 맞춘 값)
	/** 기준 크기(졸병 1.2배)일 때 줄 폭(cm). 덩치가 클수록 길 가운데로 */
	static constexpr float SpawnLaneWidth = 130.f;
	static constexpr float SpawnLaneReferenceScale = 1.2f;
	static constexpr float SpawnLaneMin = 25.f;
	static constexpr float SpawnLaneMax = 150.f;
	/** 출발점 앞뒤로 흩어지는 범위(cm) */
	static constexpr float SpawnBehind = 260.f;
	static constexpr float SpawnAhead = 60.f;
	/** 몇 군데 찍어 보고 가장 한적한 자리를 고른다 */
	static constexpr int32 SpawnTries = 12;
	/** 크기 1당 몸 반지름(cm) (겹침 판정) */
	static constexpr float BodyRadiusPerScale = 45.f;
	/** 크기 1당 머리 꼭대기 높이(cm) (위에서 떨어뜨릴 때) */
	static constexpr float HeadTopPerScale = 95.f;
	static constexpr float DropClearancePerScale = 50.f;
	static constexpr float DropClearance = 40.f;
	/** 크기 1당 서 있을 때 몸통 높이(cm) + 공중에서 시작하는 여유 */
	static constexpr float StandHeightPerScale = 92.f;
	static constexpr float SpawnLift = 30.f;
	static constexpr float GuardSpawnLift = 60.f;
	/** 같은 몫 안에서 나오는 시간을 조금씩 흩뜨린다(초) */
	static constexpr float SpawnTimeJitter = 0.25f;
	/** 푸딩 출렁임 = 기본 + 깨물기 1당 */
	static constexpr float PuddingWobbleBase = 0.5f;
	static constexpr float PuddingWobblePerBite = 0.2f;
	/** 한 프레임 실시간 최대 (멈췄다 풀릴 때 튀지 않게) */
	static constexpr float MaxRealDelta = 0.1f;

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
	int32 Gold = 0;
	int32 Lives = 0;
	int32 Wave = 0;
	int32 Kills = 0;
	int32 Drowned = 0;
	int32 Leaked = 0;
	int32 BestCombo = 0;

	TArray<FSpawn> SpawnQueue;
	int32 SpawnIndex = 0;
	float WaveTime = 0.f;

	float FlickCooldown = 0.f;

	TArray<double> RecentDowns;
	double LastComboTime = -100.0;
	float SlowMoLeft = 0.f;

	TArray<FYJJFloater> Floaters;
	FString BannerText;
	FLinearColor BannerColor = FLinearColor::White;
	float BannerAge = 0.f;
	float BannerLife = 0.f;
	float BannerSize = 0.f;
	float EndAge = 0.f;
};
