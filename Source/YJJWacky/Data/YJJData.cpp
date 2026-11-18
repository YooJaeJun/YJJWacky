#include "Data/YJJData.h"

#include "Data/YJJLoc.h"

#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "YJJWacky.h"

namespace
{
	FString DataPath(const TCHAR* File)
	{
		return FPaths::ProjectContentDir() / TEXT("YJJWacky/Data") / File;
	}

	/** CSV 파일 -> 엔진 DataTable (행 구조체는 RowStruct) */
	UDataTable* LoadTable(const TCHAR* File, UScriptStruct* RowStruct)
	{
		UDataTable* Table = NewObject<UDataTable>(GetTransientPackage(), NAME_None, RF_Transient);
		Table->RowStruct = RowStruct;
		Table->AddToRoot();
		FString Csv;
		if (!FFileHelper::LoadFileToString(Csv, *DataPath(File)))
		{
			UE_LOG(LogYJJWacky, Error, TEXT("[Data] %s 를 읽지 못했다 (%s)"), File, *DataPath(File));
			return Table;
		}
		for (const FString& Problem : Table->CreateTableFromCSVString(Csv))
		{
			UE_LOG(LogYJJWacky, Warning, TEXT("[Data] %s: %s"), File, *Problem);
		}
		UE_LOG(LogYJJWacky, Display, TEXT("[Data] %s: %d rows"), File, Table->GetRowMap().Num());
		return Table;
	}

	template <typename EnumType>
	FName EnumName(EnumType Value)
	{
		return FName(*StaticEnum<EnumType>()->GetNameStringByValue(static_cast<int64>(Value)));
	}

	struct FStore
	{
		UDataTable* Enemies = nullptr;
		UDataTable* Towers = nullptr;
		UDataTable* Waves = nullptr;
		UDataTable* Tuning = nullptr;
		TArray<FYJJWaveDef> WaveDefs;
		TSet<FString> Missing;

		void Load()
		{
			for (UDataTable* Old : { Enemies, Towers, Waves, Tuning })
			{
				if (Old)
				{
					Old->RemoveFromRoot();
				}
			}
			Enemies = LoadTable(TEXT("Enemies.csv"), FYJJEnemyRow::StaticStruct());
			Towers = LoadTable(TEXT("Towers.csv"), FYJJTowerRow::StaticStruct());
			Waves = LoadTable(TEXT("Waves.csv"), FYJJWaveRow::StaticStruct());
			Tuning = LoadTable(TEXT("Tuning.csv"), FYJJTuningRow::StaticStruct());
			Missing.Reset();
			BuildWaves();
		}

		void BuildWaves()
		{
			// 웨이브 번호별로 줄을 모은다 (CSV 줄 순서 = 몫 순서)
			WaveDefs.Reset();
			const UEnum* KindEnum = StaticEnum<EYJJWobblerKind>();
			Waves->ForeachRow<FYJJWaveRow>(TEXT("YJJData"), [&](const FName& Key, const FYJJWaveRow& Row)
			{
				if (Row.Wave < 1)
				{
					return;
				}
				if (WaveDefs.Num() < Row.Wave)
				{
					WaveDefs.SetNum(Row.Wave);
				}
				FYJJWaveDef& Def = WaveDefs[Row.Wave - 1];
				Def.BudgetKg = Row.BudgetKg;
				Def.Duration = Row.Duration;
				const int64 Kind = KindEnum->GetValueByNameString(Row.Kind);
				if (Kind == INDEX_NONE)
				{
					UE_LOG(LogYJJWacky, Warning, TEXT("[Data] Waves.csv %s: 모르는 몬스터 '%s'"), *Key.ToString(), *Row.Kind);
					return;
				}
				Def.Shares.Add({ static_cast<EYJJWobblerKind>(Kind), Row.Share, Row.From, Row.To });
			});
			for (int32 i = 0; i < WaveDefs.Num(); ++i)
			{
				if (WaveDefs[i].Shares.Num() == 0)
				{
					UE_LOG(LogYJJWacky, Warning, TEXT("[Data] Waves.csv: 웨이브 %d 이 비었다"), i + 1);
				}
			}
		}
	};

	FStore& Store()
	{
		static FStore Instance;
		if (!Instance.Enemies)
		{
			Instance.Load();
		}
		return Instance;
	}

	FAutoConsoleCommand ReloadCommand(TEXT("yjj.ReloadData"), TEXT("Content/YJJWacky/Data/*.csv 다시 읽기"), FConsoleCommandDelegate::CreateLambda([]
	{
		YJJData::Reload();
	}));
}

FLinearColor FYJJEnemyRow::LinearColor() const
{
	const FColor Parsed = FColor::FromHex(Color);
	return FLinearColor::FromSRGBColor(FColor(Parsed.R, Parsed.G, Parsed.B, 255));
}

const FYJJEnemyRow& YJJData::Enemy(EYJJWobblerKind Kind)
{
	static const FYJJEnemyRow Fallback;
	const FYJJEnemyRow* Row = Store().Enemies->FindRow<FYJJEnemyRow>(EnumName(Kind), TEXT("YJJData"), false);
	if (!Row)
	{
		UE_LOG(LogYJJWacky, Error, TEXT("[Data] Enemies.csv 에 %s 줄이 없다"), *EnumName(Kind).ToString());
	}
	return Row ? *Row : Fallback;
}

const FYJJTowerRow& YJJData::Tower(EYJJTowerType Type)
{
	static const FYJJTowerRow Fallback;
	const FYJJTowerRow* Row = Store().Towers->FindRow<FYJJTowerRow>(EnumName(Type), TEXT("YJJData"), false);
	if (!Row)
	{
		UE_LOG(LogYJJWacky, Error, TEXT("[Data] Towers.csv 에 %s 줄이 없다"), *EnumName(Type).ToString());
	}
	return Row ? *Row : Fallback;
}

float YJJData::Tune(const TCHAR* Name, float Fallback)
{
	FStore& S = Store();
	if (const FYJJTuningRow* Row = S.Tuning->FindRow<FYJJTuningRow>(FName(Name), TEXT("YJJData"), false))
	{
		return Row->Value;
	}
	if (!S.Missing.Contains(Name))
	{
		S.Missing.Add(Name);
		UE_LOG(LogYJJWacky, Warning, TEXT("[Data] Tuning.csv 에 %s 가 없다 (기본값 %g)"), Name, Fallback);
	}
	return Fallback;
}

int32 YJJData::WaveCount()
{
	return Store().WaveDefs.Num();
}

const FYJJWaveDef& YJJData::Wave(int32 WaveNumber)
{
	static const FYJJWaveDef Empty;
	const TArray<FYJJWaveDef>& Defs = Store().WaveDefs;
	return Defs.IsValidIndex(WaveNumber - 1) ? Defs[WaveNumber - 1] : Empty;
}

void YJJData::Reload()
{
	Store().Load();
	YJJLoc::Reload();
	UE_LOG(LogYJJWacky, Display, TEXT("[Data] reloaded"));
}
