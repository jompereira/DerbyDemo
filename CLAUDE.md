# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

DerbyDemo is an Unreal Engine 5 vehicle racing game with three game mode variants: basic free roam, time trial, and offroad. The codebase is C++ with UE5 reflection (UCLASS/UPROPERTY/UFUNCTION macros).

## Building

The project uses a **source-built engine** located at `C:/Projects/UnrealEngine` (registered via HKCU as `{9407D2CB-475E-D60D-5339-AEA28AD3AB51}`). It is not an Epic Games Launcher install.

**Command-line (PowerShell) — editor module:**
```powershell
& "C:\Projects\UnrealEngine\Engine\Build\BatchFiles\Build.bat" DerbyDemoEditor Win64 Development "C:\Projects\DerbyDemo\DerbyDemo.uproject" -waitmutex
```

**Command-line — game (standalone):**
```powershell
& "C:\Projects\UnrealEngine\Engine\Build\BatchFiles\Build.bat" DerbyDemo Win64 Development "C:\Projects\DerbyDemo\DerbyDemo.uproject" -waitmutex
```

**From Visual Studio 2022:**
- Open `DerbyDemo.sln` (or right-click `DerbyDemo.uproject` → "Generate Visual Studio project files")
- Build target: `DerbyDemoEditor | Development | Win64`
- Required VS components: C++ toolchain, Windows SDK 22621, Unreal debugging tools (see `.vsconfig`)

Build artifacts go to `Binaries/` and `Intermediate/`. UHT runs automatically as part of the build — if you add or change `UCLASS`/`USTRUCT`/`UFUNCTION` macros, the next build will regenerate the `*.generated.h` files.

## Running

Launch from the Unreal Editor. Three playable maps:
- `Content/VehicleTemplate/Maps/VehicleBasic.umap` — basic mode
- `Content/Variant_TimeTrial/Maps/Lvl_Timetrial.umap` — time trial
- `Content/Variant_OffRoad/Maps/Lvl_Offroad.umap` — offroad

## Module Dependencies

Declared in `Source/DerbyDemo/DerbyDemo.Build.cs`. Core dependencies: `Core`, `CoreUObject`, `Engine`, `EnhancedInput`, `ChaosVehicles`, `PhysicsCore`, `UMG`, `Slate`, `InputCore`, `GeometryCollectionEngine`.

## Architecture

### Class Hierarchy

```
ADerbyDemoPawn                      ← abstract base (vehicle pawn)
├── ADerbyDemoSportsCar             ← sports car variant
└── ADerbyDemoOffroadCar            ← offroad car variant

ADerbyDemoGameMode                  ← abstract base (no shared behavior; currently empty)
ATimeTrialGameMode                  ← inherits AGameModeBase directly; finish line lookup + lap count config
AOffroadGameMode                    ← inherits AGameModeBase directly; offroad variant

ADerbyDemoPlayerController          ← input mapping, UI spawning, respawn logic
ATimeTrialPlayerController          ← parallel controller (NOT a subclass of above)
```

**Important:** `ATimeTrialPlayerController` is a completely separate class that inherits directly from `APlayerController`, not from `ADerbyDemoPlayerController`. It duplicates input setup, touch control, and respawn logic. Any shared changes must be made in both controllers.

### Vehicle System

- All vehicles inherit from `AWheeledVehiclePawn` and use `UChaosWheeledVehicleMovementComponent`
- Input uses UE5 Enhanced Input System (actions: Steering, Throttle, Brake, Handbrake, LookAround, CameraToggle, VehicleReset)
- Each input action has a private `void Foo(FInputActionValue&)` handler that delegates to a public `BlueprintCallable` `void DoFoo(...)` method — the `Do*` methods are the canonical entry points, also used by mobile touch controls
- `BrakeLights(bool)` is a `BlueprintImplementableEvent` — brake light material/effect logic must be implemented in Blueprint, not C++
- Dual spring-arm cameras (front/rear); back camera yaw is lerped back to 0 each tick
- Flip detection uses two consecutive timer checks (`FlipCheckTime` interval, default 3 s): first failed check sets `bPreviousFlipCheck`, second triggers `DoResetVehicle()`; "flipped" is defined as the vehicle's up-dot falling below `FlipCheckMinDot` (default −0.2)
- `DisplayDebug` is overridden to add per-wheel lateral slip telemetry to the `showdebug vehicle` HUD
- Angular damping is set to 3.0 in `Tick` when airborne, 0.0 when grounded
- **Collision camera shake:** `CollisionCameraShake` (subclass of `UCameraShakeBase`) is triggered in `NotifyHit` via `DoCameraShake(NormalImpulse)`. Shake scale is linearly mapped from impulse magnitude: 0 at `CollisionShakeMinImpulse` (default 800 N·s), 1.0 at `CollisionShakeMaxImpulse` (default 5000 N·s). Configure both thresholds and the shake asset in the Blueprint Details panel
- **Geometry collection self-collision:** `BeginPlay` iterates all `UGeometryCollectionComponent` children on the vehicle and sets `ECC_Vehicle` to `ECR_Ignore`, preventing debris fragments from colliding with the vehicle's own skeletal mesh body
- Wheel variants follow a naming convention: `UDerbyDemo[Variant]Wheel[Front|Rear]`
- **Wheel tuning:** `UChaosVehicleWheel` constructor values (including `LateralSlipGraph` keys) always override anything set in the Blueprint Details panel at PIE startup. Edit wheel properties in C++, not the editor.

### Time Trial Mode

- `ATimeTrialGameMode` finds the finish line gate on `BeginPlay` by searching for `ATimeTrialTrackGate` actors with the `FinishTag` actor tag (set in the game mode's Details panel)
- Gate sequencing is entirely managed by `ATimeTrialPlayerController`, not the game mode: the controller holds a `TargetGate` pointer; each gate's `NotifyActorBeginOverlap` advances it to `NextMarker`
- `bIsFinishLine` on a gate triggers `IncrementLapCount()` on the controller when passed
- `FStartRaceDelegate` event drives the countdown UI (`UTimeTrialStartUI`) before race start

### UI System

- `UDerbyDemoUI` — base HUD showing speed (MPH/KPH) and gear; updated every tick by the player controller
- `UTimeTrialUI` — separate widget (does **not** extend `UDerbyDemoUI`); the time trial controller spawns both independently; tracks lap/best-time state
- `UTimeTrialStartUI` — sub-widget spawned by `UTimeTrialUI`; countdown animation is Blueprint-driven; calls `FinishCountdown()` (BlueprintCallable) when done, which fires `FCountdownFinishedDelegate`
- All UI uses UMG; key update points are exposed as `BlueprintImplementableEvent` for Blueprint extension

### Input

- Modern Enhanced Input (Content/Input/) is the primary system
- `ADerbyDemoPlayerController` maintains two IMC arrays: `DefaultMappingContexts` (always added) and `MobileExcludedMappingContexts` (skipped on touch platforms)
- Optional `SteeringWheelInputMappingContext` is added alongside defaults when `bUseSteeringWheelControls` is true; does not block other input
- `bForceTouchControls` (Config property) forces mobile UI on desktop for testing
- Legacy `DefaultInput.ini` bindings also present for compatibility

### Damage / Deformation System

**Data — `FMorphTargetData` struct**
- Pairs a `SocketName` (FName) with a `Durability` (float); the `MorphTargets` array on the component is configured per-Blueprint in the Details panel
- Socket names must match both a socket on the skeletal mesh **and** a morph target of the same name; the `MT_` prefix convention is used for both

**Component — `UMorphTargetsComponent`**
- Add to any vehicle Blueprint; automatically finds the owner's `USkeletalMeshComponent` on register via `FindMeshComponent()`, which falls back to `GetTypedOuter<AActor>()` when `OwnerPrivate` is null (Blueprint-editor archetype context)
- `ApplyDamage(FName SocketName, float DamageAmount)` — accumulates damage against `Durability` in `DamageCache`, then calls `SetMorphTarget` with the 0–1 blend weight; clamped so damage never exceeds `Durability`. After each call it also runs `RefreshBodyScale` and `RefreshDamageMaterial`
- `ApplyDamageAtLocation(FVector WorldHitLocation, FVector WorldHitNormal, float MaxDistance, float DamageAmount)` — finds the closest configured socket then calls `ApplyDamage`; intended as the single Blueprint call-site inside `OnImpact`
- `GetClosestMTSocket(WorldHitLocation, WorldHitNormal, MaxDistance)` — iterates `MorphTargets` (not all mesh sockets); projects both the hit location and each socket into mesh local space, then masks out the dominant axis of the hit normal so distance is compared **in the plane of impact** (not 3D). This prevents corner sockets from winning over a center socket when the physics contact point is offset in depth. Skips any socket that fails `DoesSocketExist`
- `GetMorphTargetSocketOptions()` — returns all socket names on the mesh that begin with `MorphTargetSocketPrefix` (default `"MT_"`); useful for editor dropdowns
- `bDebugDraw` — when enabled, draws each morph target's socket name and current blend weight as an orange world-space label every tick
- **Physics body shrink:** `MaxBodyShrink` (0–0.5, default 0) scales the root `FBodyInstance` by `1 − MaxBodyShrink × avgDamageRatio` after every hit. All `MT_` sockets share the root bone, so this shrinks the entire chassis hull uniformly. Set to 0 to disable
- **Material damage:** `DamageMaterialParameter` (FName, default `NAME_None`) and `DamageMaterialSlot` (int32, default 0) drive a scalar material parameter with the average panel damage ratio (0–1). A `UMaterialInstanceDynamic` is created on `BeginPlay`; leave `DamageMaterialParameter` as `NAME_None` to skip this entirely

**Pawn integration — `ADerbyDemoPawn::NotifyHit`**
- Chaos physics contact points land on convex hull corners, not on the visual mesh surface. `NotifyHit` refines the hit location by firing `LineTraceComponent` along the hit normal against `MyComp` (50 cm in each direction); the `ImpactPoint` from that trace replaces the raw contact point before it reaches `OnImpact`
- `OnImpact(FVector ImpactLocation, FVector ImpactNormal, float ImpulseMagnitude)` is a `BlueprintImplementableEvent`; Blueprint implements the deformation logic and calls `ApplyDamageAtLocation` on `UMorphTargetsComponent` passing all three values
- **Physics asset accuracy matters:** the line trace refines within whatever physics bodies exist; tighter convex bodies per panel (front bumper, rear, sides) in the Physics Asset Editor improve contact point accuracy further

### Logging

Use `LogDerbyDemo` (declared in `DerbyDemo.h`) for all project log output:
```cpp
UE_LOG(LogDerbyDemo, Error, TEXT("..."));
```

### Source Layout

```
Source/DerbyDemo/
├── DerbyDemoPawn.h/cpp             ← base vehicle
├── DerbyDemoGameMode.h/cpp         ← base game mode
├── DerbyDemoPlayerController.h/cpp ← input & UI
├── DerbyDemoUI.h/cpp               ← base HUD widget
├── DerbyDemoWheelFront/Rear.h/cpp  ← base wheels
├── SportsCar/                      ← sports car + its wheels
├── OffroadCar/                     ← offroad car + its wheels
└── Variant_TimeTrial/              ← time trial game mode, controller, gates, UI
    Variant_OffRoad/                ← offroad game mode
```

Config lives in `Config/` (DefaultEngine.ini, DefaultGame.ini, DefaultInput.ini, DefaultEditor.ini).