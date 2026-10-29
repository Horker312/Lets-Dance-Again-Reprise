# LDA:R / Dance Studio

**Let's Dance Again: Reprise(LDA:R)** 본 모드와 **Dance Studio** 편집기의 소스 저장소입니다.

| 구분 | 소스 위치 | 공개 기준 버전 | 실행 환경 |
|---|---|---|---|
| LDA:R 본 모드 | [runtime/](runtime/) | 0.5.5, Photo Mode 일시정지 수정본 | Skyrim SE **1.5.97**, SKSE64 x64 |
| Dance Studio 편집기 | [editor/](editor/) | 0.5.0 | Windows, .NET 10 WPF |
| CommonLibSSE-NG | [vendor/CommonLibSSE-NG/](vendor/CommonLibSSE-NG/) | 실제 사용한 로컬 SDK 소스 스냅샷 | SE 전용 빌드 |

[프로젝트 안내](docs/README-KO.md) · [빌드 안내](docs/BUILDING.md) · [라이선스와 외부 자료](THIRD_PARTY.md)

## 주요 기능

LDA:R은 춤과 카메라 곡 패키지를 각각 선택해 음악 샘플 위치를 기준으로 재생합니다. 준비·시작·정지, 일시정지, HUD 숨기기, 외부 카메라 모드 연동을 지원합니다.

Dance Studio는 별도 Windows 편집기입니다. 타임라인에서 춤·카메라·음악의 배치와 자르기, 배속, 카메라 설정을 조정한 다음 게임용 곡 패키지를 내보낼 수 있습니다.

## 포함하지 않는 파일

곡·음원·춤·카메라 HKX, 게임 자료, 스켈레톤 바이너리, 세이브, 실행 로그, 인증 정보와 컴파일된 프로그램은 포함하지 않습니다. 편집기의 외부 DLL과 내장 스켈레톤은 별도 빌드 입력으로 공급해야 합니다. 이들 자료의 실제 소스와 권리 확인이 모두 완료됐다는 의미는 아닙니다.

## 게임 버전 지원 범위

현재 DLL은 기본적으로 **1.5.97만 허용**합니다. 실험적으로 1.6.640과 1.6.1170의 지원을 시작했지만, 작동을 장담할 순 없습니다.

## 라이선스

자체 LDA:R/Dance Studio 소스는 **GPL-3.0-or-later**와 [EXCEPTIONS.md](EXCEPTIONS.md)의 모딩·링크 추가 허용 조건으로 공개합니다. GPL 원문은 [COPYING](COPYING)에 있습니다. 외부 파일은 각 원저작자의 라이선스를 유지하며, 이 허용 조건이 외부 스켈레톤·곡·게임 자료나 포함하지 않은 DLL의 재배포 권한을 부여하지는 않습니다.

외부 의존성과 아직 확인이 필요한 부분은 [THIRD_PARTY.md](THIRD_PARTY.md)에 구분했습니다. 특히 HKX2E/Matcher의 실제 의존성 소스를 추가로 확보해야 편집기를 외부 DLL 없이 소스만으로 다시 빌드하는 경로를 완성할 수 있습니다. 저장소 공개만으로 모든 배포 라이선스 의무가 완료됐다고 주장하지 않습니다.

## 검증 범위

공개 준비 과정에서 자체 C++·C# 소스가 기존 로컬 버전과 동일한지 해시로 확인했습니다. 대응 정보는 [docs/source-provenance.json](docs/source-provenance.json)에 있습니다. 
