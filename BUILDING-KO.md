# 빌드 안내

저장소 구조: runtime/, editor/, face_assets/, facial_studio/, vendor/CommonLibSSE-NG/. 제품 버전은 versions.json이며 Facial Studio의 독립 버전은 해당 csproj에 있습니다. 빌드만으로 버전이 증가하지 않습니다.

## 두 편집기

Windows + .NET 10 SDK, NuGet 의존성 복원이 필요합니다.

```powershell
python generate_versions.py --check
dotnet build facial_studio/App/FacialStudio.csproj -c Release
dotnet build editor/DanceStudio.csproj -c Release
```

Dance Studio의 editor/lib에 HKX2E.dll, Matcher.dll, MemoryPack.Core.dll, Riok.Mapperly.Abstractions.dll을 허가된 대응 빌드로 준비하고 editor/Resources/skeleton_female.hkx도 준비해야 합니다. 이 파일들은 저장소에 포함하지 않았습니다. Facial Studio는 NuGet의 Nifly/BCnEncoder와 공유 프리셋 프로젝트를 사용합니다.

## SKSE DLL

MSVC x64 Developer Command Prompt, Windows SDK, spdlog/fmt/DirectXTK의 include/lib, nlohmann/json와 rapidcsv 헤더가 필요합니다. 도구는 의존성을 자동 다운로드하지 않습니다.

```powershell
python tools/build_native.py --dependencies <의존성-prefix> --json-include <json-header-root> --rapidcsv-include <rapidcsv-header-root>
```

SDK와 플러그인은 ENABLE_SKYRIM_SE=1 / ENABLE_SKYRIM_AE=1 / HAS_SKYRIM_MULTI_TARGETING=1, VR 비활성 조건으로 맞춥니다. 이미 빌드한 정확히 일치하는 CommonLib 라이브러리는 --commonlib-library로 지정할 수 있습니다. 새 후킹 주소를 추정하지 않습니다.

실제 배포 후보는 로컬 빌드에서 검증했습니다. 공개 폴더 구조로의 소스 배치와 프로젝트 참조는 확인하지만, 외부 의존성을 모두 새로 설치한 독립 재현 빌드를 검증 완료로 보고하지 않습니다. SDK/게임/외부 자산 라이선스는 THIRD_PARTY.md를 함께 확인하세요.
