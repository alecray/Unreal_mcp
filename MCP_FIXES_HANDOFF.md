# McpAutomationBridge — fixes work, handoff notes

Detailed technical handoff so another agent can pick up exactly where this left off.
Author of work: Claude (Opus). Date: 2026-06-15/16.

---

## 0. TL;DR / current state

We are extending/fixing the **ChiR24/Unreal_mcp** plugin (`McpAutomationBridge`), which Alec
uses for AI-driven Unreal Editor automation. This repo is a clone of ChiR24's `dev` branch; our
work is on branch **`mcp-fixes`**, pushed to Alec's fork **`git@github.com:alecray/Unreal_mcp.git`**
(remote name `fork`). Base commit: `94acb14` (ChiR24 `dev` @ 2026-06-15, "#475 apply defaultValue").

Commits on `mcp-fixes`:
- `c63c6ba` **fix: build McpAutomationBridge on UE 5.8 (FJsonObject::Values key type)** — complete, verified (builds clean).
- `940935f` **feat: validate_niagara_system reports real stack issues (WIP)** — superseded; see below.
- **#7 now RESOLVED & VERIFIED (2026-06-16):** switched the validate path to a FULL view model
  (`bIsForDataProcessingOnly = false`). Root cause found: `UNiagaraStackModuleItem::RefreshIssues`
  (engine `NiagaraStackModuleItem.cpp:967`) hard-returns an empty issue list in data-processing mode,
  so a data-processing VM can NEVER surface per-module errors. Built in MCPBench (5.8) and HTTP-tested:
  `/Game/NS_BenchTest` → `isValid:false, errors:["The module has unmet dependencies."]`;
  `/Game/NS_Control` → `isValid:true` (no false alarm). See §5. **For a clean upstream PR, squash
  `940935f` + the finalizing commit.**

All on `fork/mcp-fixes` (pushed). Shipped fixes (each built + HTTP-verified in MCPBench on 5.8):
- `971f040` #7 validate_niagara_system (full VM) · `524d62a` #4 spawn rollback (transactional) ·
  `e02900f` #5 ListenPorts drop warning · `8613c1e` **`MCP_NATIVE_PORT` env override** (pick the native
  port per editor via env, no ini edit; mirrors existing `MCP_MAX_*` env pattern — upstream-PR-ready).
- **#11 (PCG Build.cs delay-load) DROPPED** — out of scope for WPF (PCG not enabled there). #3
  (`bEnableNativeMCP` default) parked (maintainer call). #2 (.uplugin) already on master+dev.
- **WPF MCP port drift fixed & verified:** `.mcp.json` was 8000 vs server 3010. Per-game native ports
  now: **WPF 3001, gearmaw 3002, lumespawn 3003** (client+server; lumespawn server still off). WPF
  editor launched → MCP answered an initialize handshake on 3001. Open upstream issues are all UE 5.7.x
  (low relevance to WPF/5.8).

The improvement backlog (audit of Alec's notes across all his game repos) lives in
`E:\Game Projects\gearmaw\Plugins\McpAutomationBridge\UNREAL_MCP_IMPROVEMENTS.md` (a different
repo — gearmaw — chosen because the plugin is vendored there and gearmaw is a personal project).

---

## 1. Background / the task

Alec ran an audit: "find every note/TODO I've written about Unreal MCP and compile a prioritized
improvement list, validate against the live upstream, then start fixing the ones upstream hasn't."

- Plugin actually in use: **ChiR24/Unreal_mcp → `McpAutomationBridge` v0.5.30**, vendored in-project
  at `Plugins/McpAutomationBridge/` in both `E:\Game Projects\gearmaw` (UE 5.7) and
  `E:\Game Projects\c-a-c\whenpigsfly` (UE 5.8). Native MCP HTTP server, editor-only.
- `C:\Users\ajray\unreal-mcp` is the **wrong fork** (chongdashu/unreal-mcp, the deprecated
  npm/WebSocket original) installed by an earlier agent — ignore it.
- Upstream is at v0.5.30 (released 2026-06-05); `dev` is active daily. We pinned `94acb14`.

### Upstream triage results (which backlog items are already fixed in dev @ `94acb14`)
| # | Item | Verdict @ 94acb14 |
|---|------|-------------------|
| 6 | Niagara sprite-renderer material | ✅ FIXED — `Domains/NiagaraAuthoring/...Renderers.cpp` now `LoadObject<UMaterialInterface>` + `Renderer->Material = ...` |
| 8 | Niagara motion modules no-op | ◻️ likely FIXED — `...ParticleModules.cpp` now loads real engine module assets (DragForce/CurlNoise/AddVelocity) |
| 9 | Emitter scaffold/deprecated | ◻️ likely FIXED — create path reworked into new `NiagaraAuthoring` domain |
| 13 | Blueprint CallFunction node | ✅ FIXED — `Domains/Blueprint/Graph/...AddNodeGraph.cpp:64` `NewObject<UK2Node_CallFunction>` + `SetFromFunction()` |
| 10 | Param-name normalization | ◻️ improved — `classPath` alias (#359) + alias pattern common |
| 7 | `validate_niagara_system` lies | ❌ STILL BROKEN → our WIP (§5) |
| 11 | PCG Build.cs delay-load (5.8) | ❌ STILL present — `McpAutomationBridge.Build.cs:~54` still `AddOptionalDynamicModule(...,"PCG",...)` (LNK1194 risk if PCG enabled) |
| 3 | `bEnableNativeMCP` default | ❌ still `false` — `Public/McpAutomationBridgeSettings.h:141` |
| 5 | `ListenPorts` not additive | ❌ still — `Private/Core/Settings/McpAutomationBridgeSettings.cpp:22` default `8090,8091`; partial ini override replaces wholesale |
| 4 | Spawn rollback on error | ❔ not yet read — handler in `Private/Domains/ControlActor/McpAutomationBridge_ControlActorSpawn.cpp` |
| 1 | MCP port drift across configs | n/a — fix in the GAME repos' `.mcp.json` (gearmaw/lumespawn=3001, whenpigsfly=8000, docs say 3000) |
| 2 | `.uplugin` only on a feature branch | n/a — fix in whenpigsfly git (merge descriptor to `dev`/`master`) |

So the genuinely-unfixed, worth-doing set is: **#7 (parked), #11, #5, #4, #3 (propose-only)** + the
two game-repo config items (#1, #2).

---

## 2. Repo / branch / remote layout

- Dev clone (this repo): `E:\_mcp-upstream` — shallow (`--depth 1`) clone of ChiR24/Unreal_mcp.
  - `origin` → `https://github.com/ChiR24/Unreal_mcp.git` (upstream, read-only for us)
  - `fork`   → `git@github.com:alecray/Unreal_mcp.git` (Alec's fork — push here, SSH per Alec's rule)
  - Working branch: `mcp-fixes` (tracks `fork/mcp-fixes`)
  - NOTE: shallow clone. If you need full history (e.g. rebase), `git fetch --unshallow origin`.
- The plugin source we edit: `E:\_mcp-upstream\plugins\McpAutomationBridge\Source\McpAutomationBridge\`
- Backlog doc (separate repo): `E:\Game Projects\gearmaw\Plugins\McpAutomationBridge\UNREAL_MCP_IMPROVEMENTS.md`
- **Lessons library**: `E:\claude-lessons\` (add a `ue5-...md` there for the Niagara-validate lesson — see §6).

---

## 3. The test bench: MCPBench

A throwaway UE 5.8 C++ project used to build + run the plugin in isolation (so we never touch
whenpigsfly while iterating). Once a fix is verified here, port the plugin source to whenpigsfly.

- Location: `E:\MCPBench` (blank C++ project; `MCPBench.uproject`, `Source/`, `Config/`).
- **The plugin is a DIRECTORY JUNCTION**, not a copy:
  `E:\MCPBench\Plugins\McpAutomationBridge` → `E:\_mcp-upstream\plugins\McpAutomationBridge`.
  So building the bench builds the `mcp-fixes` branch directly (single source of truth).
  Recreate if needed (PowerShell, admin not required for junctions):
  ```powershell
  New-Item -ItemType Junction -Path "E:\MCPBench\Plugins\McpAutomationBridge" -Target "E:\_mcp-upstream\plugins\McpAutomationBridge"
  ```
- **Target settings matter**: `Source/MCPBench(Editor).Target.cs` use
  `DefaultBuildSettings = BuildSettingsVersion.V7;` and `IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;`.
  Using `V5`/`Latest` fails with *"MCPBenchEditor modifies the values of properties ... not allowed,
  as it has build products in common with UnrealEditor"* (installed-engine editor target can't flip
  warning levels). Mirror whenpigsfly's targets.
- `Config/DefaultGame.ini` enables the native MCP server on an **isolated port 3010**:
  ```ini
  [/Script/McpAutomationBridge.McpAutomationBridgeSettings]
  ListenPorts=8090,8091
  bEnableNativeMCP=True
  NativeMCPPort=3010
  ```

### Build (editor MUST be closed — DLL is locked while it runs)
```powershell
$log = "$env:TEMP\mcpbench_build.log"
& "Z:\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" MCPBenchEditor Win64 Development -Project="E:\MCPBench\MCPBench.uproject" -waitmutex *> $log
Write-Output "EXITCODE=$LASTEXITCODE"
(Select-String -Path $log -Pattern '^Result:|error C|error LNK|fatal error|: error').Line | Select-Object -Last 20
```
First full plugin build ~30 min; incrementals ~2 min. Run `Build.bat` via **PowerShell** (path has a
space). Close any UE_5.8 editor first: `Get-Process UnrealEditor | ? { $_.Path -like '*UE_5.8*' } | Stop-Process -Force`.

### Run + test the MCP server directly over HTTP (no Claude Code MCP registration needed)
Launch the editor (`Start-Process "Z:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList '"E:\MCPBench\MCPBench.uproject"'`),
wait for `localhost:3010` to listen, then drive the plugin's native MCP server with raw HTTP. This
avoids registering the server in Claude Code (which would need a restart and lose session context).

```powershell
$base = "http://localhost:3010/mcp"
function Text($c){ ($c -split "`n" | ? { $_ -like "data:*" } | % { try { ($_.Substring(5).Trim() | ConvertFrom-Json).result.content[0].text } catch { $_ } }) -join "`n" }
# 1) initialize -> capture session id from the RESPONSE HEADER
$init = @{ jsonrpc="2.0"; id=1; method="initialize"; params=@{ protocolVersion="2025-03-26"; capabilities=@{}; clientInfo=@{ name="bench"; version="1.0" } } } | ConvertTo-Json -Depth 6 -Compress
$r1 = Invoke-WebRequest -Uri $base -Method Post -Body $init -ContentType "application/json" -Headers @{ Accept="application/json, text/event-stream" } -UseBasicParsing -TimeoutSec 30
$sid = $r1.Headers["Mcp-Session-Id"]; $h = @{ Accept="application/json, text/event-stream"; "Mcp-Session-Id"=$sid }
# 2) initialized notification (required)
Invoke-WebRequest -Uri $base -Method Post -Body (@{ jsonrpc="2.0"; method="notifications/initialized" } | ConvertTo-Json -Compress) -ContentType "application/json" -Headers $h -UseBasicParsing | Out-Null
# 3) tools/call. Responses are SSE ("data:" lines). Tool = "manage_effect"; sub-action in arguments.action
$vb = @{ jsonrpc="2.0"; id=3; method="tools/call"; params=@{ name="manage_effect"; arguments=@{ action="validate_niagara_system"; systemPath="/Game/NS_BenchTest" } } } | ConvertTo-Json -Depth 10 -Compress
Text (Invoke-WebRequest -Uri $base -Method Post -Body $vb -ContentType "application/json" -Headers $h -UseBasicParsing -TimeoutSec 120).Content
```
Tool param notes (read from source, not guessed):
- `create_niagara_system` args: `name`, `path` (folder, e.g. `/Game/`), `save` (bool). NOT `systemPath`.
- `validate_niagara_system` arg: `systemPath` (full content path, e.g. `/Game/NS_BenchTest`).
- **PowerShell footgun:** never name a function parameter `$args` — it collides with the automatic
  `$args` variable and silently sends empty arguments. (Cost us a confusing empty-result run.)

### Test asset
`/Game/NS_BenchTest` exists in the bench, saved in a **deliberately broken** state: a "Fountain"
emitter with its **Emitter State** module deleted, so "Spawn Rate" reports
*"The module has unmet dependencies"* (pre-dependency `EmitterLifeCycle` not met; needs `EmitterState`).
The editor shows this as a red error. This is the asset to validate against for the negative test.
`/Game/NS_Control` is a clean empty system (positive/no-false-alarm control).

---

## 4. What's done & verified

### #fix RenderConsole 5.8 (commit c63c6ba) — DONE
File: `.../Domains/Render/McpAutomationBridge_RenderConsole.cpp` (~line 43).
UE 5.8 changed `FJsonObject::Values`' key type to `UE::TSharedString<TCHAR>`, so
`Settings->Values.Contains(Field)` (FString) no longer compiles. Replaced with the public accessor
`Settings->HasField(Field)`. **This was the ONLY 5.8 compile break in the whole plugin** — WPF's old
0.5.30 patches (Lighting `EngineTypes.h`, Test `JsonFields`) are NOT needed on dev. Good upstream PR.

---

## 5. #7 validate_niagara_system — RESOLVED (history below; was parked WIP)

> **RESOLUTION (2026-06-16):** The blocker below ("data-processing VM doesn't run per-emitter
> dependency analysis") was traced to the engine: `UNiagaraStackModuleItem::RefreshIssues`
> (`NiagaraStackModuleItem.cpp:967`) does `if (GetSystemViewModel()->GetIsForDataProcessingOnly()) {
> NewIssues.Empty(); return; }` — module items emit NO issues at all in data-processing mode. The fix
> was **Option A**: build a FULL view model (`bIsForDataProcessingOnly = false`) with `bCanSimulate=false`
> (so `SetupPreviewComponentAndInstance` makes no preview component) and compile off; `SetupSequencer`
> still runs but only builds a detached transient Sequencer. Option B (reuse the open editor's VM) was
> confirmed IMPOSSIBLE from the plugin: `NiagaraSystemToolkit.h` is in NiagaraEditor/**Private** and the
> `TNiagaraViewModelManager` static won't link. So we always spin our own VM. Verified via HTTP against
> `/Game/NS_BenchTest` (isValid:false + unmet-dependency) and `/Game/NS_Control` (isValid:true). The
> historical analysis below is kept for context.

File: `.../Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersInfoValidation.cpp`
Function: `ValidateNiagaraSystem` (+ file-static helper `CollectStackIssues`).

### The problem we're fixing
Upstream `validate_niagara_system` **hard-codes `isValid=true`** and only emits soft structural
warnings (no emitters / disabled / no renderers). It never detects real breakage.

### Wrong approach #1 (discarded): script compile status
First attempt inspected each `UNiagaraScript::GetLastCompileStatus()` for `NCS_Error`. **This misses
the common failures.** "Unmet dependencies" and "deprecated module" are **Niagara STACK ISSUES**
(`EStackIssueSeverity::Error`), computed by the editor's stack view model — NOT VM script-compile
errors. A broken emitter can still have `NCS_UpToDate` scripts. (Verified: this approach returned
`isValid:true` on the broken asset.)

### Current approach (in commit 940935f): harvest stack issues from a view model
`CollectStackIssues(UNiagaraStackEntry* Root, ...)` recurses `GetUnfilteredChildren()` and reads
`Entry->GetIssues()`, routing `EStackIssueSeverity::Error` → errors, `Warning` → warnings (uses
`GetShortDescription().ToString()`). `ValidateNiagaraSystem`:
1. builds a **headless** `FNiagaraSystemViewModel` (`MakeShared` + `Options.bIsForDataProcessingOnly = true`,
   `bCanAutoCompile/bCanSimulate/bCompileForEdit/bCanModifyEmittersFromTimeline = false`,
   `EditMode = SystemAsset`, **`MessageLogGuid = FGuid::NewGuid()`**), `Initialize(*System, Options)`, `RefreshAll()`.
2. For the system stack and each emitter stack: `GetRootEntry()->RefreshChildren()` then `CollectStackIssues`.
3. `isValid = ErrorsArray.Num() == 0`.

### Why it's PARKED — the remaining bug
The headless **data-processing-only** view model **does NOT run the per-emitter dependency analysis**.
On the broken `NS_BenchTest` it returns `isValid:true, errors:[], warnings:[]`. The harvest mechanism
itself WORKS (the clean control system surfaced a real system-level stack issue
*"Object is not transactional, undo won't work for it!"*), but the emitter dependency check
(`NiagaraStackModuleItem` — see engine ref below) doesn't fire in data-processing mode. The editor's
**full** view model does run it (confirmed by opening the asset — red error shows).

### Hard constraints discovered (so you don't repeat them)
- **`MessageLogGuid` is REQUIRED**: without it, `RefreshAll()` asserts
  `MessageAssetKey != FGuid()` (`NiagaraMessageManager.cpp:291` "Tried to subscribe to an asset
  without a set asset key") → editor crash. We set a throwaway `FGuid::NewGuid()`.
- **`Cleanup()` is NOT exported** (no `NIAGARAEDITOR_API`) → can't call it from the plugin module.
  But `~FNiagaraSystemViewModel()` calls `Cleanup()` internally, so **just let the `TSharedRef` drop**
  (this is what the engine's own headless helpers do). No leak.
- **Can't reuse an already-open editor view model the easy way**:
  `TNiagaraViewModelManager<UNiagaraSystem,FNiagaraSystemViewModel>::GetExistingViewModelForObject`
  fails to LINK from the plugin module — `LNK2001 unresolved external ... ObjectsToViewModels`
  (a private static member of the template, only instantiated inside NiagaraEditor). We removed that call.
- MCPBench targets need `V7`/`Unreal5_8` (see §3).

### Recommended next options (pick one — was about to ask Alec)
- **Option A (recommended to try first): full view model.** Set `Options.bIsForDataProcessingOnly = false`.
  This builds the full stacks like the editor (`SetupPreviewComponentAndInstance` + undo registration),
  which DOES run the dependency analysis. Risk: heavier; possible headless crash; if the asset is also
  open in a Niagara editor you'd have two full VMs (conflict). The destructor still cleans up. One
  rebuild to test against `NS_BenchTest`. **Mitigation for the "asset already open" case:** guard by
  checking `GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorsForAsset(System)` and, if
  open, reuse that editor's VM instead (see Option B).
- **Option B: reuse the open editor's VM** via
  `GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorsForAsset(System)` → cast the
  `IAssetEditorInstance*` to `FNiagaraSystemToolkit` → get its system view model (verify the accessor
  is exported; `FNiagaraSystemToolkit` is in `NiagaraEditor/Public/`). Reliable & exact, but
  validate only catches errors when the asset is currently open in an editor.
- **Option C: replicate the dependency check directly** without a view model, via
  `FNiagaraStackGraphUtilities` dependency utilities (the check that NiagaraStackModuleItem uses).
  Most self-contained but most code.
- A robust shipping answer is probably **A + the B guard** (reuse if open, else full headless).

### Engine source references (UE 5.8) — read these before coding
Base: `Z:\Epic Games\UE_5.8\Engine\Plugins\FX\Niagara\Source\NiagaraEditor\`
- `Public/ViewModels/NiagaraSystemViewModel.h` — `Initialize(UNiagaraSystem&, FNiagaraSystemViewModelOptions)`,
  `RefreshAll()`, `GetSystemStackViewModel()`, `GetEmitterHandleViewModels()`; `FNiagaraSystemViewModelOptions`
  fields (`bIsForDataProcessingOnly`, `bCanAutoCompile`, `bCompileForEdit`, `MessageLogGuid`, `EditMode`).
  Dtor (`Private/...NiagaraSystemViewModel.cpp:378`) calls `Cleanup()`. `RefreshAll()` body at `.cpp:2039`
  (calls `RefreshEmitterHandleViewModels()` + `SystemStackViewModel->InitializeWithViewModels(...)`).
  In `Initialize`, line ~123/178 gate undo/preview on `!bIsForDataProcessingOnly`; SystemStackViewModel
  object is created unconditionally (line ~140).
- `Public/ViewModels/NiagaraEmitterHandleViewModel.h` — `GetEmitterStackViewModel()`. Its `Initialize`
  (`.cpp:112`) calls `EmitterStackViewModel->InitializeWithViewModels(...)` (line 120).
- `Public/ViewModels/Stack/NiagaraStackViewModel.h` — `GetRootEntry()`, `InitializeWithViewModels(...)`.
- `Public/ViewModels/Stack/NiagaraStackEntry.h` — `GetIssues()` (line 520), `GetUnfilteredChildren()`,
  `RefreshChildren()` (line 445, exported), `GetTotalNumberOfErrorIssues()` (516), `struct FStackIssue`
  (`GetSeverity`, `GetShortDescription`, `GetLongDescription`), `enum EStackIssueSeverity { Error=0, Warning, Info, None }`.
- `Private/ViewModels/Stack/NiagaraStackModuleItem.cpp:903` — where "The module has unmet dependencies."
  is generated (the exact check to satisfy/replicate; long desc names the unmet `FNiagaraModuleDependency`).
- `Private/NiagaraEditorUtilities.cpp:5101` `CreateDataProcessingOnlyViewModel` — canonical headless pattern.
- Build.cs already lists `NiagaraEditor` (optional module) so its `NIAGARAEDITOR_API` symbols link.
  Includes needed (guarded `#if WITH_EDITOR`): `ViewModels/NiagaraSystemViewModel.h`,
  `ViewModels/NiagaraEmitterHandleViewModel.h`, `ViewModels/Stack/NiagaraStackViewModel.h`,
  `ViewModels/Stack/NiagaraStackEntry.h` (already added to the .cpp).

---

## 6. Lessons learned (also worth a file in E:\claude-lessons\)
1. **UE 5.8 `FJsonObject::Values` key type → `UE::TSharedString<TCHAR>`.** `.Values.Contains/Find(FString)`
   no longer compiles; use `HasField()`/`TryGetField()` accessors. Likely affects other UE plugins on 5.8.
2. **Niagara: "unmet dependencies"/deprecated-module errors are STACK ISSUES, not VM compile errors.**
   To detect "what the editor shows as an error", harvest `UNiagaraStackEntry::GetIssues()`
   (`EStackIssueSeverity::Error`) from the stack view model — NOT `UNiagaraScript::GetLastCompileStatus()`.
3. **Headless `FNiagaraSystemViewModel`:** must set `Options.MessageLogGuid` (else `RefreshAll()` asserts);
   don't call the unexported `Cleanup()` (the destructor does it); **`bIsForDataProcessingOnly = true`
   does NOT compute per-emitter dependency issues** — need the full view model (or the open editor's).
4. **Cross-module template static link error:** `TNiagaraViewModelManager::GetExistingViewModelForObject`
   won't link from a plugin (its static `ObjectsToViewModels` lives only in NiagaraEditor). Use
   `UAssetEditorSubsystem::FindEditorsForAsset` to reach an open toolkit instead.
5. **Installed-engine editor targets** must not change global warning levels: use `BuildSettingsVersion.V7`
   + `EngineIncludeOrderVersion.Unreal5_8`, not `V5`/`Latest`.
6. **Test the plugin's MCP server over raw HTTP** (initialize → `Mcp-Session-Id` header → SSE `data:`
   lines) to avoid registering it in Claude Code and losing session context. PowerShell: never name a
   param `$args`.
7. **Iterate in a junctioned throwaway project** (MCPBench), not in the shipping game project. Close the
   editor before every build (locked DLL). Cap build output to a log and grep errors.

---

## 7. Next steps (prioritized)
1. ~~**Finish #7**~~ ✅ DONE & VERIFIED (2026-06-16) — Option A full VM; no Option B guard (impossible
   from the plugin, see §5). Built in MCPBench, HTTP-tested both broken + clean. Finalizing commit on
   `mcp-fixes` (local; not yet pushed). **Open question for a shipping build:** if the asset is already
   open in a Niagara editor, we now spin a *second* full VM for the same `UNiagaraSystem` — theoretical
   conflict (both register undo / reset). Not observed; revisit only if it bites. NEXT BACKLOG ITEM ↓.
2. ~~**#11 PCG Build.cs 5.8**~~ ⏭️ DROPPED (2026-06-16, Alec's call) — OUT OF SCOPE for whenpigsfly.
   The fix (`AddOptionalDynamicModule(...,"PCG",..)` → `AddOptionalConditionalModule(EngineDir,"PCG","PCG")`
   on `Build.cs:54`, dropping the delay-load DLL to avoid LNK1194) is real but only matters when the PCG
   plugin is enabled. whenpigsfly does NOT enable PCG (its `.uproject` has no PCG; `bHasPCG`=false →
   delay-load line never runs → `MCP_HAS_PCG=0` → no LNK1194). Niagara is independent of PCG (Niagara's
   only plugin dep is PythonScriptPlugin), so WPF using Niagara does not transitively need it. Keep as a
   possible **upstream-only** PR (helps anyone who enables PCG on 5.8); not implemented here.
3. ~~**#5 ListenPorts additive**~~ ✅ DONE & VERIFIED (2026-06-16, commit `e02900f`, local). Decision
   (Alec asked for a rec): chose **warn-only**, not force-additive — additive would re-bind the
   defaults even when a user dropped them on purpose (a surprising, outward-facing network change),
   whereas warn changes zero binding behavior. `FMcpConnectionManager::Initialize` now logs a Warning
   when `bMultiListen` is on and the configured `ListenPorts` omits a default bridge port (8090/8091).
   Verified in MCPBench: `ListenPorts=9000` → warning naming 8090,8091 fires AND the server still binds
   only 9000 (no forced extra sockets).
4. ~~**#4 spawn rollback**~~ ✅ DONE & VERIFIED (2026-06-16, commit `524d62a`, local). `HandleControlActorSpawn`
   is now transactional: pre-spawn `MESH_NOT_FOUND` if an explicit `meshPath` can't load; post-spawn
   `Destroy()` + `MESH_APPLY_FAILED` if a resolved mesh can't be applied (mirrors the spline handlers).
   Verified in MCPBench over HTTP (happy/static-mesh, bad-mesh, PointLight+mesh rollback with PointLight
   count returning to baseline, plain-PointLight regression).
5. **#3 bEnableNativeMCP default** — propose-only (flipping a server-on default is a security/design call;
   raise upstream rather than just flipping).
6. **Game-repo config items** (not in this plugin): #1 reconcile the MCP port across
   gearmaw/lumespawn/whenpigsfly `.mcp.json` + docs; #2 merge the `.uplugin` descriptor onto
   whenpigsfly's `dev`/`master` so branch switches don't kill MCP.
7. **Port verified fixes to whenpigsfly:** copy the changed plugin source files from this clone into
   `E:\Game Projects\c-a-c\whenpigsfly\Plugins\McpAutomationBridge\...`, build `WhenPigsFlyEditor` on
   UE 5.8 (editor closed), pair-test with Alec, commit per WPF PR rules (feature branch → PR into `dev`,
   `--base dev`, reviewer esker, follow the PR template).
8. **Upstream PRs** for the generally-useful fixes (RenderConsole 5.8 now; validate once it actually
   detects dependency errors). Branch is already on `fork` (`alecray/Unreal_mcp`) ready to PR to ChiR24.

---

## 8. How to resume the bench quickly
1. `git -C E:\_mcp-upstream status` (should be on `mcp-fixes`).
2. Ensure the junction exists (§3). Edit plugin source under `E:\_mcp-upstream\plugins\McpAutomationBridge\...`.
3. Close any UE_5.8 editor → build (§3) → relaunch editor → wait for `localhost:3010` → test over HTTP (§3).
4. Broken test asset: `/Game/NS_BenchTest`. Clean control: `/Game/NS_Control`.
5. Commit per logical task (feature-tagged), `Co-Authored-By: Claude ...`, push to `fork`.
