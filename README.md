# LDA:R / Dance Studio / Facial Studio

현재 소스: **LDA:R 0.7.1 RC / Dance Studio 0.6.1 RC / Facial Studio 0.6.0**

|구성|소스|주요 변경|
|---|---|---|
|LDA:R|[runtime](runtime)|최대 5인 공연, NPC 강제 배치·간격을 둔 위치 유지, 복구 큐와 재배치 충돌 방지|
|Dance Studio|[editor](editor)|독립 춤·카메라·표정 타임라인, 성별 하위 폴더 전용 모션 출력, 파일·출력 안정화|
|Facial Studio|[facial_studio](facial_studio)|RaceMenu 자동 얼굴 연결, 원자적 경로 재연결, 자동 안정 ID, 얼굴 미리보기 조작|

[최신 변경·검증](docs/LATEST-RELEASE-KO.md) · [빌드](BUILDING-KO.md) · [라이선스와 외부 의존성](THIRD_PARTY.md)

## 사용과 호환성

- 플레이어·NPC를 합쳐 최대 5명이며 NPC-only 공연을 지원합니다. F10 지정·배치·준비·재생, ESC 배치 취소, 공연 중 Space 일시정지를 유지합니다.
- 같은 셀의 NPC는 AI·전투·Scene 참여만으로 배치를 취소하지 않고 간격을 두어 복귀시킵니다. 공연 중에는 위치 고정을 해제합니다. 원래 퀘스트 Scene의 완전한 재개는 보장하지 않습니다.
- 새 곡은 female/mt_idle.hkx와 male/mt_idle.hkx만 생성합니다. 기존 최상위 모션 참조·해시는 사용하지 않으며 하위 파일의 무결성 검증은 유지합니다.
- Steam SE 1.5.97 / 1.6.640 / 1.6.1170 대상 빌드입니다. 실제 Skyrim/MO2 및 모든 버전의 플레이 검증 완료를 뜻하지 않습니다.
- 기존 플레이어 전용 곡은 NPC 공연을 위해 명시적으로 재출력이 필요합니다. 원본 HKX·음악·개인 프로젝트를 자동 변경하지 않습니다.

## 공개 범위

현재 반영은 소스와 한국어 안내입니다. 과거 dist 파일은 파일명에 적힌 구버전입니다. 최신 실행 파일은 이번에 GitHub Releases/dist에 추가하지 않았습니다. 외부 DLL과 내장 XPMSSE의 정확한 재배포 허가 확인이 남아 있습니다.

곡·음악·개인 .jslot·NIF/TRI/DDS·스켈레톤 바이너리·인증 정보·사용자 설정은 올리지 않습니다. **내장 스켈레톤 기능은 소스에 유지**하며 실제 빌드 입력은 사용자가 허가된 원본으로 준비해야 합니다.

자체 소스의 기존 GPL 라이선스와 [추가 허용 조건](EXCEPTIONS.md), 각 외부 코드의 고지를 유지합니다. 저장소 갱신만으로 모든 외부 의존성의 소스·재배포 조건이 충족됐다고 주장하지 않습니다.
