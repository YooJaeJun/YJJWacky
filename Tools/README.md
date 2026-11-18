# YJJWacky 도구

엔진: `D:\source\repos\UnrealEngine` (UE 5.8.1 소스 빌드, YJJJump·YJJFantasy와 같은 엔진)

## 빌드

```
D:\source\repos\UnrealEngine\Engine\Build\BatchFiles\Build.bat YJJWackyEditor Win64 Development -project="D:\source\repos\YJJWacky\YJJWacky.uproject" -WaitMutex
```

## 콘텐츠 (에디터를 끈 상태에서)

```
powershell -File Tools/Content/BuildContent.ps1
```

- `Tools/Audio/generate_audio.py` : 효과음·배경음 합성 (외부 음원 없음)
- `Tools/Content/build_content.py` : 머티리얼(단색 표면·반투명·물), 소리 임포트, 빈 맵

전장·캐릭터·타워는 에셋이 아니라 코드가 게임 시작 때 조립한다.
캐릭터에는 애니메이션이 없다. 전부 물리다 (`Source/YJJWacky/Physics/YJJWobbler.cpp`).

## 데이터 테이블 (수치·글자)

게임 수치와 화면 글자는 코드가 아니라 `Content/YJJWacky/Data/*.csv` 에 있다. 엑셀로 열어 고치면 된다 (UTF-8 BOM으로 저장).
게임이 시작할 때 CSV를 엔진 DataTable로 읽는다. 게임 중 콘솔 `yjj.ReloadData` 를 치면 끄지 않고 다시 읽는다.

| 파일 | 내용 | 행 이름 |
| --- | --- | --- |
| `Enemies.csv` | 몬스터: 크기, 체력, 속도, 현상금, 깨물기, **무게**, 색 | `EYJJWobblerKind` (Grunt, Runner, Brute, Boss, Tiny, Guard) |
| `Towers.csv` | 타워: 값, 사거리, 강화 비용 비율, 단계별 `Power`/`Cooldown`/`ParamA`/`ParamB` (뜻은 `Note` 열) | `EYJJTowerType` (Punch, Cannon, Fan, Guard, Sling) |
| `Waves.csv` | 웨이브마다 총 무게(`BudgetKg`), 시간, 몬스터별 몫·나오는 구간 | 아무 이름 (W01_Grunt ...) |
| `Tuning.csv` | 경제·떼죽음·딱밤·타워 힘·흐느적이 물리 감각 | 이름 (`Game.StartGold`, `Wobbler.UprightStiffness` ...) |
| `Localization.csv` | 화면 글자. 열 = `Key, ko, en, Note` | 키 (`Hud.Start` ...) |

- 언어: 명령줄 `-yjjlang=en` 또는 콘솔 `yjj.Lang en`. 없으면 시스템 언어, 그것도 없으면 한국어. 언어를 늘리려면 열을 하나 붙인다 (예: `ja`).
- 서식 자리는 `{0}`, `{1}` ... 빠진 키는 화면에 `[키]` 로 보이고 로그에 경고가 남는다.
- 몸 치수·애니메이션 모양처럼 "그림"에 해당하는 값은 코드에 남겨 두었다.

## 실행·테스트

```
powershell -File Tools/RunGame.ps1               # 그냥 플레이
powershell -File Tools/RunGame.ps1 -Mode play    # 자동 플레이 검증 -> 로그 [AutoPilot] PASS/FAIL, Saved/Screenshots/Play_*.png
powershell -File Tools/RunGame.ps1 -Mode play -Tests towers,upgrade -Waves 10   # 일부만 (walk, towers, upgrade, stress, waves)
```

게임 중 콘솔(`~`): `yjj.AutoPlay`, `yjj.Gold 500`, `yjj.ReloadData`, `yjj.Lang en`

## 게임: 흐느적 디펜스

흐느적거리는 빨간 놈들이 둑길을 따라 푸딩을 뜯어 먹으러 온다. 길에는 난간이 없다.
길옆 발판에 타워를 지어 길 밖(물)으로 날려 버리면 된다. 10 웨이브를 버티면 승리.

| 타워 | 값 | 하는 일 | Lv2 | Lv3 |
| --- | --- | --- | --- | --- |
| 펀치 타워 | 60 | 권투 장갑(깜짝 상자)이 길을 가로질러 튀어나가 줄 선 적을 통째로 날린다 | 왕장갑, 1.35배 | 연타 + 1.75배 |
| 대포 | 110 | 멀리서 진짜 물리 포탄을 쏜다. 굴러가며 쓰러뜨리고 한 번 터진다 | 큰 포탄, 넓게 | 세 발 동시 |
| 선풍기 | 80 | 켜졌다 꺼졌다 하며 길 건너편으로 바람을 분다 | 강풍 | 회오리: 가벼운 놈을 공중으로 띄움 |
| 몽둥이 병사 | 70 | 파란 흐느적이가 길목에서 빙글빙글 몽둥이를 돌린다. 돌고 나면 어지러워한다 | 큰 몽둥이 | 징 박힌 몽둥이 |
| 새총 | 90 | 무게 한도(Lv1 70kg: 졸병까지) 안의 적을 올가미로 낚아채, 고무줄을 당겼다가 앞선 무리에 탄알로 쏜다. 표적이 없으면 물로 던진다. 너무 무거우면 "무거워!" | 200kg까지 (뚱보도) | 520kg까지 (대장도) |

강화: 지은 타워를 좌클릭. 비용은 Lv2 = 값 x0.9, Lv3 = 값 x1.5. 판매가는 들인 돈의 60%. 발판 뒤 금색 구슬 수 = 단계.

## 레벨 디자인: 무게

모든 수치의 중심은 몬스터 무게다. 맞고 날아가는 거리는 무게에 반비례(졸병 60kg 기준), 바람은 무게^0.8에 반비례.
웨이브는 "총 무게 예산"과 몬스터별 몫으로 짠다 (`Content/YJJWacky/Data/Waves.csv`).
같은 무게라도 꼬마 떼(가볍고 많음)와 뚱보(무겁고 적음)는 전혀 다른 문제를 낸다.

| 적 | 무게 | 특징 |
| --- | --- | --- |
| 꼬마 | 15kg | 아주 작고 가볍다. 바람에 훨훨, 새총 탄알 1순위 |
| 날쌘돌이 | 35kg | 빠르고 가볍다 (멀리 날아간다) |
| 졸병 | 60kg | 기준 |
| 뚱보 | 180kg | 잘 안 날아간다. 강화한 타워나 새총 Lv2가 필요 |
| 대장 | 500kg | 거대. 새총 Lv3만 든다. 5·10 웨이브 |

| 웨이브 | 총 무게 | 구성 |
| --- | --- | --- |
| 1 | 480kg | 졸병 |
| 2 | 720kg | 졸병 + 꼬마 |
| 3 | 1,000kg | 졸병 + 날쌘돌이 + 꼬마 |
| 4 | 1,350kg | 졸병 + 뚱보 + 꼬마 |
| 5 | 1,800kg | 날쌘돌이·졸병·꼬마 + 대장 1 |
| 6~9 | 2,200~4,000kg | 점점 뚱보 비중이 커진다 |
| 10 | 5,000kg | 졸병·뚱보·꼬마 + 대장 3 |

한꺼번에 다섯 이상 쓰러뜨리면 "떼죽음" 슬로 모션 + 보너스 골드. 새총 탄알이 셋 이상 맞히면 "스트라이크!".

## 조작

빈 발판 좌클릭 짓기 · 지은 타워 좌클릭 강화 · 우클릭 팔기 · 적 좌클릭 딱밤(카메라 반대쪽으로 튕김) · 1~5 타워 고르기
Space 다음 웨이브 · WASD/방향키 이동 · 휠 확대 · Q/E 회전 · R 다시 하기(끝난 뒤)
