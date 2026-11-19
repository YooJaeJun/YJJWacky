#include "Data/YJJLoc.h"

#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/Csv/CsvParser.h"
#include "YJJWacky.h"

bool YJJLoc::bLoaded = false;
TMap<FString, TMap<FString, FString>> YJJLoc::Texts;
TArray<FString> YJJLoc::Languages;
FString YJJLoc::Current;
TMap<FString, FString> YJJLoc::MissingShown;
FAutoConsoleCommand YJJLoc::LangCommand(TEXT("yjj.Lang"), TEXT("언어 바꾸기: yjj.Lang en"), FConsoleCommandWithArgsDelegate::CreateStatic(&YJJLoc::SetLanguageFromConsole));

void YJJLoc::Load()
{
	Texts.Reset();
	Languages.Reset();
	MissingShown.Reset();
	bLoaded = true;
	const FString Path = FPaths::ProjectContentDir() / FilePath;
	FString Csv;
	if (!FFileHelper::LoadFileToString(Csv, *Path))
	{
		UE_LOG(LogYJJWacky, Error, TEXT("[Loc] %s 를 읽지 못했다"), *Path);
		return;
	}
	const FCsvParser Parser(MoveTemp(Csv));
	const FCsvParser::FRows& Rows = Parser.GetRows();
	if (Rows.Num() == 0)
	{
		return;
	}
	// 머리줄: Key, ko, en, ..., Note
	TArray<int32> LanguageColumns;
	for (int32 c = 1; c < Rows[0].Num(); ++c)
	{
		const FString Header = FString(Rows[0][c]).TrimStartAndEnd();
		if (!Header.Equals(NoteColumn, ESearchCase::IgnoreCase) && !Header.IsEmpty())
		{
			Languages.Add(Header.ToLower());
			LanguageColumns.Add(c);
		}
	}
	for (int32 r = 1; r < Rows.Num(); ++r)
	{
		const TArray<const TCHAR*>& Row = Rows[r];
		const FString Key = Row.Num() > 0 ? FString(Row[0]).TrimStartAndEnd() : FString();
		if (Key.IsEmpty() || Key.StartsWith(CommentPrefix))
		{
			continue;
		}
		if (Texts.Contains(Key))
		{
			UE_LOG(LogYJJWacky, Warning, TEXT("[Loc] 키 중복: %s"), *Key);
		}
		TMap<FString, FString>& Entry = Texts.FindOrAdd(Key);
		for (int32 i = 0; i < LanguageColumns.Num(); ++i)
		{
			const int32 c = LanguageColumns[i];
			if (c < Row.Num() && *Row[c])
			{
				Entry.Add(Languages[i], Row[c]);
			}
		}
	}
	UE_LOG(LogYJJWacky, Display, TEXT("[Loc] %d keys, languages: %s"), Texts.Num(), *FString::Join(Languages, TEXT(", ")));
}

void YJJLoc::PickLanguage()
{
	// 기본은 한국어. 다른 언어는 명령줄 -yjjlang=en 또는 콘솔 yjj.Lang en 으로만 (시스템 언어를 따르지 않는다)
	FString Wanted = DefaultLanguage;
	FParse::Value(FCommandLine::Get(), LanguageSwitch, Wanted);
	Wanted = Wanted.ToLower();
	Current = Languages.Contains(Wanted) ? Wanted : FString(DefaultLanguage);
}

void YJJLoc::EnsureLoaded()
{
	if (!bLoaded)
	{
		Load();
		PickLanguage();
	}
}

const FString& YJJLoc::Get(const TCHAR* Key)
{
	EnsureLoaded();
	if (const TMap<FString, FString>* Entry = Texts.Find(Key))
	{
		if (const FString* Text = Entry->Find(Current))
		{
			return *Text;
		}
		if (const FString* Fallback = Entry->Find(DefaultLanguage))
		{
			return *Fallback;
		}
	}
	// 빠진 글자는 [키] 로 보여 줘서 바로 눈에 띄게 한다
	FString& Shown = MissingShown.FindOrAdd(Key);
	if (Shown.IsEmpty())
	{
		Shown = FString::Printf(TEXT("[%s]"), Key);
		UE_LOG(LogYJJWacky, Warning, TEXT("[Loc] Localization.csv 에 %s 가 없다"), Key);
	}
	return Shown;
}

FString YJJLoc::Format(const TCHAR* Key, const FStringFormatOrderedArguments& Args)
{
	return FString::Format(*Get(Key), Args);
}

const FString& YJJLoc::Language()
{
	EnsureLoaded();
	return Current;
}

void YJJLoc::SetLanguage(const FString& InLanguage)
{
	EnsureLoaded();
	const FString Wanted = InLanguage.ToLower();
	if (Languages.Contains(Wanted))
	{
		Current = Wanted;
		UE_LOG(LogYJJWacky, Display, TEXT("[Loc] language %s"), *Wanted);
	}
	else
	{
		UE_LOG(LogYJJWacky, Warning, TEXT("[Loc] 없는 언어 %s (있는 언어: %s)"), *Wanted, *FString::Join(Languages, TEXT(", ")));
	}
}

void YJJLoc::SetLanguageFromConsole(const TArray<FString>& Args)
{
	if (Args.Num() > 0)
	{
		SetLanguage(Args[0]);
	}
}

void YJJLoc::Reload()
{
	const FString Keep = Current;
	Load();
	Current = Languages.Contains(Keep) ? Keep : FString(DefaultLanguage);
}
