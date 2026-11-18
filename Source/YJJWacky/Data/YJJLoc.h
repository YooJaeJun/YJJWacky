#pragma once

#include "CoreMinimal.h"

/**
 * 화면에 나오는 모든 글자는 Content/YJJWacky/Data/Localization.csv 에 있다.
 *   열: Key, ko, en, Note  (언어를 늘리려면 열을 하나 더 붙이면 된다. 예: ja)
 * 언어: 명령줄 -yjjlang=en > 콘솔 yjj.Lang en > 시스템 언어 > ko.
 * 그 언어 칸이 비어 있으면 ko, 그것도 없으면 [Key] 를 보여 준다 (빠진 글자가 눈에 띄게).
 * 서식 자리는 {0} {1} ... (FString::Format)
 */
namespace YJJLoc
{
	const FString& Get(const TCHAR* Key);
	FString Format(const TCHAR* Key, const FStringFormatOrderedArguments& Args);

	/** 지금 언어 (ko, en ...) */
	const FString& Language();
	void SetLanguage(const FString& Language);

	/** CSV를 다시 읽는다 (yjj.ReloadData 도 함께 부른다) */
	void Reload();
}
