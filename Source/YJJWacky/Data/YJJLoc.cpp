#include "Data/YJJLoc.h"

#include "HAL/IConsoleManager.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/Csv/CsvParser.h"
#include "YJJWacky.h"

namespace
{
	struct FTextStore
	{
		/** 키 -> (언어 -> 글자) */
		TMap<FString, TMap<FString, FString>> Texts;
		TArray<FString> Languages;
		FString Language;
		bool bLoaded = false;
		TMap<FString, FString> MissingShown;

		void Load()
		{
			Texts.Reset();
			Languages.Reset();
			MissingShown.Reset();
			bLoaded = true;
			const FString Path = FPaths::ProjectContentDir() / TEXT("YJJWacky/Data/Localization.csv");
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
				if (!Header.Equals(TEXT("Note"), ESearchCase::IgnoreCase) && !Header.IsEmpty())
				{
					Languages.Add(Header.ToLower());
					LanguageColumns.Add(c);
				}
			}
			for (int32 r = 1; r < Rows.Num(); ++r)
			{
				const TArray<const TCHAR*>& Row = Rows[r];
				const FString Key = Row.Num() > 0 ? FString(Row[0]).TrimStartAndEnd() : FString();
				if (Key.IsEmpty() || Key.StartsWith(TEXT("#")))
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

		void PickLanguage()
		{
			FString Wanted;
			if (!FParse::Value(FCommandLine::Get(), TEXT("yjjlang="), Wanted))
			{
				Wanted = FInternationalization::Get().GetCurrentCulture()->GetTwoLetterISOLanguageName();
			}
			Wanted = Wanted.ToLower();
			Language = Languages.Contains(Wanted) ? Wanted : FString(TEXT("ko"));
		}
	};

	FTextStore& Store()
	{
		static FTextStore Instance;
		if (!Instance.bLoaded)
		{
			Instance.Load();
			Instance.PickLanguage();
		}
		return Instance;
	}

	FAutoConsoleCommand LangCommand(TEXT("yjj.Lang"), TEXT("언어 바꾸기: yjj.Lang en"), FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() > 0)
		{
			YJJLoc::SetLanguage(Args[0]);
		}
	}));
}

const FString& YJJLoc::Get(const TCHAR* Key)
{
	FTextStore& S = Store();
	if (const TMap<FString, FString>* Entry = S.Texts.Find(Key))
	{
		if (const FString* Text = Entry->Find(S.Language))
		{
			return *Text;
		}
		if (const FString* Korean = Entry->Find(TEXT("ko")))
		{
			return *Korean;
		}
	}
	// 빠진 글자는 [키] 로 보여 줘서 바로 눈에 띄게 한다
	FString& Shown = S.MissingShown.FindOrAdd(Key);
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
	return Store().Language;
}

void YJJLoc::SetLanguage(const FString& InLanguage)
{
	FTextStore& S = Store();
	const FString Wanted = InLanguage.ToLower();
	if (S.Languages.Contains(Wanted))
	{
		S.Language = Wanted;
		UE_LOG(LogYJJWacky, Display, TEXT("[Loc] language %s"), *Wanted);
	}
	else
	{
		UE_LOG(LogYJJWacky, Warning, TEXT("[Loc] 없는 언어 %s (있는 언어: %s)"), *Wanted, *FString::Join(S.Languages, TEXT(", ")));
	}
}

void YJJLoc::Reload()
{
	FTextStore& S = Store();
	const FString Keep = S.Language;
	S.Load();
	S.Language = S.Languages.Contains(Keep) ? Keep : FString(TEXT("ko"));
}
