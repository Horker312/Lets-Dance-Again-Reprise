# 빌드 안내

## LDA:R 본 모드 — Skyrim SE 1.5.97 전용

Windows x64, Visual Studio 2022 C++ 도구, Windows SDK, Python 3가 필요합니다. 원래 빌드에는 MSVC 14.44를 사용했습니다. **x64 MSVC 개발자 명령 프롬프트**에서 실행하세요.

fmt, 외부 fmt를 사용하도록 설정한 spdlog, DirectXTK의 `include/`와 `lib/`가 있는 의존성 폴더를 준비합니다. `nlohmann/json.hpp`와 `rapidcsv.h`의 포함 경로도 필요합니다. 스크립트가 의존성을 자동 다운로드하지는 않습니다. Release `/MD` 라이브러리를 일관되게 사용하세요.

```powershell
python tools/build_native.py --dependencies C:/dev/deps/installed --json-include C:/dev/json/single_include --rapidcsv-include C:/dev/rapidcsv/src
```

`--commonlib-library`를 생략하면 저장소에 포함한 SDK의 소스 목록을 **ENABLE_SKYRIM_SE=1만 지정하여** 컴파일하고 `build/native/DanceStudio.dll`을 만듭니다. 몇 분 이상 걸릴 수 있으며 `--jobs`로 SDK 컴파일 작업 수를 조절할 수 있습니다.

이미 빌드한 **정확히 대응하는 SE 전용** SDK 라이브러리가 있다면 다음 옵션을 추가합니다.

```powershell
--commonlib-library C:/dev/CommonLibSSE-SE.lib
```

다중 런타임 또는 AE용 SDK 라이브러리로 바꾸지 마세요. 헤더와 라이브러리의 구조체 배치가 같아야 합니다.

출력과 빌드 로그는 Git에서 제외한 `build/native/`에 저장됩니다. 컴파일러·PDB 경로와 타임스탬프에 따라 바이너리는 달라질 수 있으므로 기존 배포 DLL과 바이트 단위로 동일한 빌드라고 주장하지 않습니다. 원래 소스와의 동일성은 `source-provenance.json`에서 따로 확인합니다.

공개 준비 과정에서는 포함된 헤더와 기존의 대응 SE 전용 SDK 라이브러리로 본 모드를 재빌드했습니다. **SDK 전체 재컴파일 경로와 새 컴퓨터의 의존성 초기 설치까지 다시 검증한 것은 아닙니다.**

`data/`에는 텍스트 설정 예시만 있습니다. 실제 게임 설치에 필요한 데이터 ESL 등은 정상 전체 모드 설치본에서 확보하세요. 이 폴더만으로는 완전한 설치본이 되지 않습니다.

## Dance Studio 편집기 — 0.4.10

Windows의 .NET 10 SDK를 사용합니다. 원래 프로젝트는 외부 DLL 4개를 참조하고 스켈레톤을 내장합니다. 해당 외부 바이너리를 이 Git 저장소에 재배포하지 않습니다.

사용 권한이 있는 원래 빌드 입력을 준비한 뒤 실행하세요.

```powershell
python tools/import_editor_inputs.py --libraries C:/private/editor-libs --skeleton C:/private/skeleton_female.hkx
dotnet build editor/DanceStudio.csproj -c Release
dotnet publish editor/DanceStudio.csproj -c Release --self-contained false -o build/editor
```

가져오기 도구는 `external-editor-inputs.json`의 해시를 확인한 뒤 Git에서 제외한 `editor/lib/`와 `editor/Resources/`에 파일을 복사합니다. 외부 업로드는 하지 않습니다.

필요한 DLL은 HKX2E.dll, Matcher.dll, MemoryPack.Core.dll, Riok.Mapperly.Abstractions.dll입니다. 프로그램 실행에는 .NET 10 Desktop Runtime이 필요합니다.

자체 C# 소스는 기존 파일 그대로 공개했습니다. 외부 자료가 없을 때 스켈레톤이나 디코더 기능을 몰래 삭제해서 빌드하지 않습니다. Matcher/HKX2E의 실제 의존성 소스를 아직 확보하지 못했으므로 **외부 DLL 없이 소스만으로 편집기를 다시 만드는 경로는 미완성**입니다. 이름이 같은 upstream 라이브러리를 원래 DLL과 동일하다고 간주하지 마세요. 자세한 경계는 THIRD_PARTY.md에 있습니다.

## 테스트와 입력 자료

C++ 및 C# 테스트 소스를 포함했습니다. 일부 테스트는 게임 또는 원본 HKX/WAV 자료가 필요하며, 이전 진단 코드에는 원래 MO2 자료 경로를 사용하는 부분도 남아 있습니다. 이 테스트 자료는 공개하지 않습니다. 깨끗한 저장소 복사본에서 외부 자료 없이 모든 테스트가 실행된다는 뜻은 아닙니다.

소스 공개 도구는 Skyrim 게임 플레이를 검증하지 않습니다. AE 지원이나 모든 모드 조합에서 지연이 없는 오디오·카메라 동기화를 보장하지 않습니다.
