#include "Data/YJJData.h"

#include "Data/YJJLoc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "YJJWacky.h"

bool YJJData::bLoaded = false;
TObjectPtr<UDataTable> YJJData::Enemies;
TObjectPtr<UDataTable> YJJData::Towers;
TObjectPtr<UDataTable> YJJData::Waves;
TObjectPtr<UDataTable> YJJData::Tuning;
TObjectPtr<UDataTable> YJJData::Sounds;
TObjectPtr<UDataTable> YJJData::Effects;
TObjectPtr<UDataTable> YJJData::TextStyles;
TObjectPtr<UDataTable> YJJData::Colors;
TObjectPtr<UDataTable> YJJData::PartsTable;
TObjectPtr<UDataTable> YJJData::LowPolyTable;
TObjectPtr<UDataTable> YJJData::SceneryTable;
TObjectPtr<UDataTable> YJJData::MapPath;
TObjectPtr<UDataTable> YJJData::MapPads;
TObjectPtr<UDataTable> YJJData::HudLayout;
TObjectPtr<UDataTable> YJJData::HudIcons;
int32 YJJData::LoadCount = 0;
TArray<FYJJWaveDef> YJJData::WaveDefs;
TSet<FString> YJJData::Warned;
const FYJJEnemyRow YJJData::DefaultEnemy;
const FYJJTowerRow YJJData::DefaultTower;
const FYJJTextStyleRow YJJData::DefaultTextStyle;
const FYJJWaveDef YJJData::EmptyWave;
FAutoConsoleCommand YJJData::ReloadCommand(TEXT("yjj.ReloadData"), TEXT("Content/YJJWacky/Data/*.csv 다시 읽기"), FConsoleCommandDelegate::CreateStatic(&YJJData::Reload));

float FYJJEnemyRow::Heft() const
{
	return WeightKg / FMath::Max(YJJData::Tune(TEXT("Game.ReferenceWeight")), 1.f);
}

FLinearColor FYJJEnemyRow::LinearColor() const
{
	return YJJData::ParseColor(Color);
}

FLinearColor FYJJEffectRow::LinearColor() const
{
	return YJJData::ParseColor(Color);
}

FLinearColor FYJJTextStyleRow::LinearColor() const
{
	return YJJData::ParseColor(Color);
}

FLinearColor YJJData::ParseColor(const FString& Hex)
{
	const FColor Parsed = FColor::FromHex(Hex);
	return FLinearColor::FromSRGBColor(FColor(Parsed.R, Parsed.G, Parsed.B, MAX_uint8));
}

UDataTable* YJJData::LoadTable(const TCHAR* File, UScriptStruct* RowStruct)
{
	// CSV 파일 -> 엔진 DataTable (행 구조체는 RowStruct)
	UDataTable* Table = NewObject<UDataTable>(GetTransientPackage(), NAME_None, RF_Transient);
	Table->RowStruct = RowStruct;
	Table->AddToRoot();
	const FString Path = FPaths::ProjectContentDir() / DataDir / File;
	FString Csv;
	if (!FFileHelper::LoadFileToString(Csv, *Path))
	{
		UE_LOG(LogYJJWacky, Error, TEXT("[Data] %s 를 읽지 못했다 (%s)"), File, *Path);
		return Table;
	}
	for (const FString& Problem : Table->CreateTableFromCSVString(Csv))
	{
		UE_LOG(LogYJJWacky, Warning, TEXT("[Data] %s: %s"), File, *Problem);
	}
	UE_LOG(LogYJJWacky, Display, TEXT("[Data] %s: %d rows"), File, Table->GetRowMap().Num());
	return Table;
}

void YJJData::Load()
{
	for (UDataTable* Old : { Enemies.Get(), Towers.Get(), Waves.Get(), Tuning.Get(), Sounds.Get(), Effects.Get(), TextStyles.Get(),
		Colors.Get(), PartsTable.Get(), LowPolyTable.Get(), SceneryTable.Get(), MapPath.Get(), MapPads.Get(), HudLayout.Get(), HudIcons.Get() })
	{
		if (Old)
		{
			Old->RemoveFromRoot();
		}
	}
	bLoaded = true;
	Enemies = LoadTable(TEXT("Enemies.csv"), FYJJEnemyRow::StaticStruct());
	Towers = LoadTable(TEXT("Towers.csv"), FYJJTowerRow::StaticStruct());
	Waves = LoadTable(TEXT("Waves.csv"), FYJJWaveRow::StaticStruct());
	Tuning = LoadTable(TEXT("Tuning.csv"), FYJJTuningRow::StaticStruct());
	Sounds = LoadTable(TEXT("Sounds.csv"), FYJJSoundRow::StaticStruct());
	Effects = LoadTable(TEXT("Effects.csv"), FYJJEffectRow::StaticStruct());
	TextStyles = LoadTable(TEXT("TextStyles.csv"), FYJJTextStyleRow::StaticStruct());
	Colors = LoadTable(TEXT("Colors.csv"), FYJJColorRow::StaticStruct());
	PartsTable = LoadTable(TEXT("Parts.csv"), FYJJPartRow::StaticStruct());
	LowPolyTable = LoadTable(TEXT("LowPoly.csv"), FYJJLowPolyRow::StaticStruct());
	SceneryTable = LoadTable(TEXT("Scenery.csv"), FYJJSceneryRow::StaticStruct());
	MapPath = LoadTable(TEXT("MapPath.csv"), FYJJPathPointRow::StaticStruct());
	MapPads = LoadTable(TEXT("MapPads.csv"), FYJJPadRow::StaticStruct());
	HudLayout = LoadTable(TEXT("HudLayout.csv"), FYJJTuningRow::StaticStruct());
	HudIcons = LoadTable(TEXT("HudIcons.csv"), FYJJIconRow::StaticStruct());
	LoadCount++;
	Warned.Reset();
	BuildWaves();
}

void YJJData::EnsureLoaded()
{
	if (!bLoaded)
	{
		Load();
	}
}

void YJJData::BuildWaves()
{
	// 웨이브 번호별로 줄을 모은다 (CSV 줄 순서 = 몫 순서)
	WaveDefs.Reset();
	const UEnum* KindEnum = StaticEnum<EYJJWobblerKind>();
	Waves->ForeachRow<FYJJWaveRow>(Context, [KindEnum](const FName& Key, const FYJJWaveRow& Row)
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
		FYJJWaveShare& Share = Def.Shares.AddDefaulted_GetRef();
		Share.Kind = static_cast<EYJJWobblerKind>(Kind);
		Share.Share = Row.Share;
		Share.From = Row.From;
		Share.To = Row.To;
	});
	for (int32 i = 0; i < WaveDefs.Num(); ++i)
	{
		if (WaveDefs[i].Shares.Num() == 0)
		{
			UE_LOG(LogYJJWacky, Warning, TEXT("[Data] Waves.csv: 웨이브 %d 이 비었다"), i + 1);
		}
	}
}

void YJJData::WarnOnce(const FString& What)
{
	if (!Warned.Contains(What))
	{
		Warned.Add(What);
		UE_LOG(LogYJJWacky, Warning, TEXT("[Data] %s"), *What);
	}
}

const FYJJEnemyRow& YJJData::Enemy(EYJJWobblerKind Kind)
{
	EnsureLoaded();
	const FYJJEnemyRow* Row = Enemies->FindRow<FYJJEnemyRow>(EnumName(Kind), Context, false);
	if (!Row)
	{
		WarnOnce(FString::Printf(TEXT("Enemies.csv 에 %s 줄이 없다"), *EnumName(Kind).ToString()));
	}
	return Row ? *Row : DefaultEnemy;
}

const FYJJTowerRow& YJJData::Tower(EYJJTowerType Type)
{
	EnsureLoaded();
	const FYJJTowerRow* Row = Towers->FindRow<FYJJTowerRow>(EnumName(Type), Context, false);
	if (!Row)
	{
		WarnOnce(FString::Printf(TEXT("Towers.csv 에 %s 줄이 없다"), *EnumName(Type).ToString()));
	}
	return Row ? *Row : DefaultTower;
}

float YJJData::Tune(const TCHAR* Name)
{
	EnsureLoaded();
	if (const FYJJTuningRow* Row = Tuning->FindRow<FYJJTuningRow>(FName(Name), Context, false))
	{
		return Row->Value;
	}
	WarnOnce(FString::Printf(TEXT("Tuning.csv 에 %s 가 없다"), Name));
	return 0.f;
}

float YJJData::Layout(const TCHAR* Name)
{
	EnsureLoaded();
	if (const FYJJTuningRow* Row = HudLayout->FindRow<FYJJTuningRow>(FName(Name), Context, false))
	{
		return Row->Value;
	}
	WarnOnce(FString::Printf(TEXT("HudLayout.csv 에 %s 가 없다"), Name));
	return 0.f;
}

TArray<const FYJJIconRow*> YJJData::Icon(const FString& Icon)
{
	EnsureLoaded();
	TArray<const FYJJIconRow*> Result;
	HudIcons->ForeachRow<FYJJIconRow>(Context, [&Result, &Icon](const FName& Key, const FYJJIconRow& Row)
	{
		if (Row.Icon == Icon)
		{
			Result.Add(&Row);
		}
	});
	return Result;
}

int32 YJJData::TuneInt(const TCHAR* Name)
{
	return FMath::RoundToInt(Tune(Name));
}

const FYJJSoundRow* YJJData::Sound(const TCHAR* Cue)
{
	EnsureLoaded();
	const FYJJSoundRow* Row = Sounds->FindRow<FYJJSoundRow>(FName(Cue), Context, false);
	if (!Row)
	{
		WarnOnce(FString::Printf(TEXT("Sounds.csv 에 %s 가 없다"), Cue));
	}
	return Row;
}

const FYJJEffectRow* YJJData::Effect(const TCHAR* Name)
{
	EnsureLoaded();
	const FYJJEffectRow* Row = Effects->FindRow<FYJJEffectRow>(FName(Name), Context, false);
	if (!Row)
	{
		WarnOnce(FString::Printf(TEXT("Effects.csv 에 %s 가 없다"), Name));
	}
	return Row;
}

const FYJJTextStyleRow& YJJData::TextStyle(const TCHAR* Name)
{
	EnsureLoaded();
	const FYJJTextStyleRow* Row = TextStyles->FindRow<FYJJTextStyleRow>(FName(Name), Context, false);
	if (!Row)
	{
		WarnOnce(FString::Printf(TEXT("TextStyles.csv 에 %s 가 없다"), Name));
	}
	return Row ? *Row : DefaultTextStyle;
}

int32 YJJData::WaveCount()
{
	EnsureLoaded();
	return WaveDefs.Num();
}

const FYJJWaveDef& YJJData::Wave(int32 WaveNumber)
{
	EnsureLoaded();
	return WaveDefs.IsValidIndex(WaveNumber - 1) ? WaveDefs[WaveNumber - 1] : EmptyWave;
}

FLinearColor YJJData::Color(const FString& NameOrHex)
{
	EnsureLoaded();
	if (const FYJJColorRow* Row = Colors->FindRow<FYJJColorRow>(FName(*NameOrHex), Context, false))
	{
		FLinearColor Result = ParseColor(Row->Hex);
		Result.A = Row->Alpha;
		return Result;
	}
	// 이름도 아니고 16진수(RRGGBB)도 아니면 표에서 빠진 것
	const bool bHex = NameOrHex.Len() == HexLength && NameOrHex.FindLastCharByPredicate([](TCHAR Ch) { return !FChar::IsHexDigit(Ch); }) == INDEX_NONE;
	if (!bHex)
	{
		WarnOnce(FString::Printf(TEXT("Colors.csv 에 %s 가 없다"), *NameOrHex));
	}
	return ParseColor(NameOrHex);
}

TArray<const FYJJPartRow*> YJJData::Parts(const FString& Model)
{
	EnsureLoaded();
	TArray<const FYJJPartRow*> Result;
	PartsTable->ForeachRow<FYJJPartRow>(Context, [&Result, &Model](const FName& Key, const FYJJPartRow& Row)
	{
		if (Row.Model == Model)
		{
			Result.Add(&Row);
		}
	});
	if (Result.Num() == 0)
	{
		WarnOnce(FString::Printf(TEXT("Parts.csv 에 %s 모델이 없다"), *Model));
	}
	return Result;
}

bool YJJData::HasParts(const FString& Model)
{
	EnsureLoaded();
	bool bFound = false;
	PartsTable->ForeachRow<FYJJPartRow>(Context, [&bFound, &Model](const FName& Key, const FYJJPartRow& Row)
	{
		bFound = bFound || Row.Model == Model;
	});
	return bFound;
}

TArray<const FYJJLowPolyRow*> YJJData::LowPoly(const FString& Model)
{
	EnsureLoaded();
	TArray<const FYJJLowPolyRow*> Result;
	LowPolyTable->ForeachRow<FYJJLowPolyRow>(Context, [&Result, &Model](const FName& Key, const FYJJLowPolyRow& Row)
	{
		if (Row.Model == Model)
		{
			Result.Add(&Row);
		}
	});
	if (Result.Num() == 0)
	{
		WarnOnce(FString::Printf(TEXT("LowPoly.csv 에 %s 모델이 없다"), *Model));
	}
	return Result;
}

TArray<const FYJJSceneryRow*> YJJData::Scenery(const FString& Kind)
{
	EnsureLoaded();
	TArray<const FYJJSceneryRow*> Result;
	SceneryTable->ForeachRow<FYJJSceneryRow>(Context, [&Result, &Kind](const FName& Key, const FYJJSceneryRow& Row)
	{
		if (Row.Kind == Kind)
		{
			Result.Add(&Row);
		}
	});
	return Result;
}

TArray<FVector> YJJData::PathPoints()
{
	EnsureLoaded();
	TArray<const FYJJPathPointRow*> Rows;
	MapPath->ForeachRow<FYJJPathPointRow>(Context, [&Rows](const FName& Key, const FYJJPathPointRow& Row)
	{
		Rows.Add(&Row);
	});
	Rows.StableSort([](const FYJJPathPointRow& A, const FYJJPathPointRow& B) { return A.Order < B.Order; });
	TArray<FVector> Result;
	for (const FYJJPathPointRow* Row : Rows)
	{
		Result.Add(FVector(Row->X, Row->Y, 0.f));
	}
	return Result;
}

TArray<FYJJPadRow> YJJData::PadRows()
{
	EnsureLoaded();
	TArray<FYJJPadRow> Result;
	MapPads->ForeachRow<FYJJPadRow>(Context, [&Result](const FName& Key, const FYJJPadRow& Row)
	{
		Result.Add(Row);
	});
	return Result;
}

int32 YJJData::Generation()
{
	EnsureLoaded();
	return LoadCount;
}

void YJJData::Reload()
{
	Load();
	YJJLoc::Reload();
	UE_LOG(LogYJJWacky, Display, TEXT("[Data] reloaded"));
}
