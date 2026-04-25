# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

DerbyDemo is an Unreal Engine 5 vehicle racing game with three game mode variants: basic free roam, time trial, and offroad. The codebase is C++ with UE5 reflection (UCLASS/UPROPERTY/UFUNCTION macros).

## Building

- Open `DerbyDemo.sln` in Visual Studio 2022 and build the `DerbyDemo` project
- Or right-click `DerbyDemo.uproject` → "Generate Visual Studio project files", then build from VS
- Required VS components are listed in `.vsconfig` (C++ toolchain, Windows SDK 22621, Unreal debugging tools)
- Build artifacts go to `Binaries/` and `Intermediate/`

## Running

Launch from the Unreal Editor. Three playable maps:
- `Content/VehicleTemplate/Maps/VehicleBasic.umap` — basic mode
- `Content/Variant_TimeTrial/Maps/Lvl_Timetrial.umap` — time trial
- `Content/Variant_OffRoad/Maps/Lvl_Offroad.umap` — offroad

## Module Dependencies

Declared in `Source/DerbyDemo/DerbyDemo.Build.cs`. Core dependencies: `Core`, `CoreUObject`, `Engine`, `EnhancedInput`, `ChaosVehicles`, `PhysicsCore`, `UMG`, `Slate`, `InputCore`.

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

- `UMorphTargetsComponent` (ClassGroup=Destruction) maps named sockets on the vehicle's `USkeletalMeshComponent` to morph targets for deformation on impact
- Sockets eligible for damage are identified by the `MorphTargetSocketPrefix` (default `"MT_"`); the same prefix is mirrored in `ADerbyDemoPawn::DamageSocketPrefix`
- `GetClosestMTSocket(WorldHitLocation, MaxDistance)` returns the nearest eligible socket to a hit point — intended to be called from Blueprint inside `OnImpact` to drive the geometry collection or morph target blend weight
- `OnImpact(FVector ImpactLocation, float ImpulseMagnitude)` is a `BlueprintImplementableEvent` on `ADerbyDemoPawn`; it fires inside `NotifyHit` alongside the camera shake — mesh deformation logic must be implemented in Blueprint, not C++
- `FMorphTargetData` (struct) pairs a socket name (with editor dropdown via `GetOptions="GetMorphTargetSocketOptions"`) with a `Durability` float; the array is editable per-actor in the Details panel

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