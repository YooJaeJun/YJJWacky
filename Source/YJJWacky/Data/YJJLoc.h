#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

/**
 * 화면에 나오는 모든 글자는 Content/YJJWacky/Data/Localization.csv 에 있다.
 *   열: Key, ko, en, Note  (언어를 늘리려면 열을 하나 더 붙이면 된다. 예: ja)
 * 언어: 기본 ko. 명령줄 -yjjlang=en 또는 콘솔 yjj.Lang en 으로 바꾼다.
 * 그 언어 칸이 비어 있으면 ko, 그것도 없으면 [Key] 를 보여 준다 (빠진 글자가 눈에 띄게).
 * 서식 자리는 {0} {1} ... (FString::Format)
 */
class YJJLoc
{
public:
	static const FString& Get(const TCHAR* Key);
	static FString Format(const TCHAR* Key, const FStringFormatOrderedArguments& Args);

	/** 지금 언어 (ko, en ...) */
	static const FString& Language();
	static void SetLanguage(const FString& Language);

	/** CSV를 다시 읽는다 (yjj.ReloadData 도 함께 부른다) */
	static void Reload();

private:
	static constexpr const TCHAR* FilePath = TEXT("YJJWacky/Data/Localization.csv");
	static constexpr const TCHAR* DefaultLanguage = TEXT("ko");
	static constexpr const TCHAR* NoteColumn = TEXT("Note");
	static constexpr const TCHAR* CommentPrefix = TEXT("#");
	static constexpr const TCHAR* LanguageSwitch = TEXT("yjjlang=");

	static void EnsureLoaded();
	static void Load();
	static void PickLanguage();
	static void SetLanguageFromConsole(const TArray<FString>& Args);

	static bool bLoaded;
	/** 키 -> (언어 -> 글자) */
	static TMap<FString, TMap<FString, FString>> Texts;
	static TArray<FString> Languages;
	static FString Current;
	static TMap<FString, FString> MissingShown;
	static FAutoConsoleCommand LangCommand;
};
