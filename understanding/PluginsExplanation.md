# VexRiscv Plugin System: Usage and Construction

## What "plugin" means in this project

In this repository, "plugin" primarily means **CPU hardware plugins** used to assemble a VexRiscv core out of modular features.
This is not an IDE plugin system and not an SBT build-plugin system.

Relevant places:

- `src/main/scala/vexriscv/plugin/` contains the hardware plugin implementations.
- `src/main/scala/vexriscv/Pipeline.scala` contains the generic plugin lifecycle and stage interconnect logic.
- `src/main/scala/vexriscv/VexRiscv.scala` shows how a CPU instance receives a list of plugins.
- `src/main/scala/vexriscv/demo/` contains ready-made CPU configurations built by composing plugins.
- `README.md` documents the public plugin model and several major plugins.

There is also a `project/plugins.sbt` file, but in this repository it is empty, so there is no meaningful SBT plugin setup to document.

## Core idea

VexRiscv is intentionally built as a **plugin-composed pipeline CPU**.
The base `VexRiscv` component defines the pipeline stages and basic arbitration, but most CPU behavior is injected by plugins:

- instruction fetch
- decode rules
- register file behavior
- ALU, shifter, multiply, divide
- hazards and bypassing
- data/instruction buses
- CSR, exceptions, interrupts
- MMU/PMP/debug/FPU/custom extensions

This gives the project two important properties:

1. You can create many CPU variants without editing the core pipeline implementation.
2. New features can be added by writing a plugin instead of forking the whole CPU.

## Architecture of the plugin framework

### 1. The `Plugin` trait

The base trait is in `src/main/scala/vexriscv/plugin/Plugin.scala`.

Each plugin:

- extends `Plugin[VexRiscv]`
- receives a back-reference to the pipeline instance
- implements two phases:
  - `setup(pipeline)`
  - `build(pipeline)`

Meaning:

- `setup` is for negotiation and registration:
  - request services from other plugins
  - publish decode defaults
  - add IOs
  - register jump/exception/CSR hooks
- `build` is for actual hardware generation:
  - insert signals into stages
  - wire logic
  - drive outputs
  - create areas inside pipeline stages

### 2. The `Pipeline` lifecycle

`src/main/scala/vexriscv/Pipeline.scala` is the real orchestration point.

The sequence is:

1. `plugins.foreach(_.pipeline = this)`
2. `plugins.foreach(_.setup(...))`
3. plugin names/scopes are reflected into the SpinalHDL hierarchy
4. `plugins.foreach(_.build(...))`
5. the pipeline automatically interconnects `Stageable`s across stages
6. stage arbitration, flush, stall, and validity propagation are finalized

This order matters:

- during `setup`, plugins can discover and configure each other
- during `build`, the shared stageable network and stage logic are emitted

### 3. Services between plugins

Plugins interact through `pipeline.service(...)`, `serviceExist(...)`, and `serviceElse(...)`.

Examples of services used throughout the repo:

- `DecoderService`
- `CsrInterface`
- `ExceptionService`
- `JumpService`
- hazard-related and privilege-related services

This is how the design avoids hard-coding dependencies between plugins.

### 4. Stageables and automatic transport

A major mechanism in VexRiscv is the use of `Stageable[T]`.

Plugins can:

- insert a value into one stage
- read it in later stages
- override or observe it along the way

`Pipeline.build()` computes where each stageable is inserted, consumed, and forwarded, then generates the required registers and defaults automatically.

This is one of the key reasons the plugin model scales: plugins communicate through typed pipeline metadata instead of direct point-to-point wiring everywhere.

## How a CPU is constructed from plugins

CPU construction is done through `VexRiscvConfig` in `src/main/scala/vexriscv/VexRiscv.scala`.

Important pieces:

- `VexRiscvConfig.plugins` is an `ArrayBuffer[Plugin[VexRiscv]]`
- `VexRiscv(config)` copies `config.plugins` into the live pipeline
- the stage set itself is also configurable:
  - `withMemoryStage`
  - `withWriteBackStage`

### Typical construction pattern

The normal pattern is:

1. Create a `VexRiscvConfig`
2. Pass a `List(...)` of plugin instances
3. Instantiate `new VexRiscv(config)`
4. Generate RTL with `SpinalVerilog(...)`

Example sources:

- `src/main/scala/vexriscv/demo/GenSmallest.scala`
- `src/main/scala/vexriscv/demo/GenFull.scala`
- `src/main/scala/vexriscv/demo/Linux.scala`

### Small configuration example

`GenSmallest.scala` assembles a minimal CPU with:

- `IBusSimplePlugin`
- `DBusSimplePlugin`
- `CsrPlugin`
- `DecoderSimplePlugin`
- `RegFilePlugin`
- `IntAluPlugin`
- `SrcPlugin`
- `LightShifterPlugin`
- `HazardSimplePlugin`
- `BranchPlugin`
- `YamlPlugin`

### Full configuration example

`GenFull.scala` shows a much richer composition:

- cached I-bus and D-bus
- MMU
- decode/regfile/ALU/source plugins
- barrel shifter
- hazards with full bypassing
- multiply/divide
- CSR/debug/branch
- YAML export

This is the clearest illustration of the design philosophy: the CPU is a selected bundle of plugins, not a monolithic block.

## How to create a new plugin

The repository already contains two direct examples:

- `src/main/scala/vexriscv/demo/CustomInstruction.scala`
- `src/main/scala/vexriscv/demo/CustomCsrDemoPlugin.scala`

### Pattern for a custom instruction plugin

The `SimdAddPlugin` example in `CustomInstruction.scala` shows the standard recipe:

1. Extend `Plugin[VexRiscv]`
2. Define any custom `Stageable`s the plugin needs
3. In `setup`:
   - get the `DecoderService`
   - register decode defaults
   - add a decode rule for the new instruction
4. In `build`:
   - place logic in a chosen pipeline stage, usually `execute plug new Area`
   - read source operands from stage inputs
   - compute the result
   - drive `REGFILE_WRITE_DATA` or other standard outputs

This is the canonical method for ISA extensions.

### Pattern for a custom CSR plugin

The `CustomCsrDemoPlugin` example shows the CSR-oriented pattern:

1. Extend `Plugin[VexRiscv]`
2. In `build`, obtain `pipeline.service(classOf[CsrInterface])`
3. Register readable/writable CSRs with:
   - `rw(...)`
   - `r(...)`
   - `onWrite(...)`
   - `onRead(...)`
4. Connect those CSRs to internal registers or external peripheral logic

This is how project-specific counters, control registers, or CSR-mapped peripherals are added.

## Practical rules for writing plugins in this codebase

### Prefer `setup` for registration, `build` for logic

Good use of `setup`:

- declare IOs
- query services
- register decoder defaults
- create jump or exception endpoints

Good use of `build`:

- create `Area`s on stages
- connect `Stageable`s
- implement datapath/control logic

### Reuse standard stageables whenever possible

The config defines common stageables in `VexRiscvConfig`, such as:

- `RS1`, `RS2`
- `PC`
- `INSTRUCTION`
- `REGFILE_WRITE_VALID`
- `REGFILE_WRITE_DATA`
- `BYPASSABLE_EXECUTE_STAGE`
- `BYPASSABLE_MEMORY_STAGE`

Using the common stageables keeps plugins interoperable with hazards, writeback, decode, and bypass logic.

### Depend on services instead of hard-wiring plugin instances

If a plugin needs decode, CSR, exception, or privilege behavior, the expected pattern is to request a service from the pipeline rather than reach directly into unrelated plugin internals.

### Treat plugin order as meaningful

The framework is dynamic, but plugin order still matters in practice:

- services must exist by the time dependent plugins query them
- some plugins expect certain base infrastructure to be present
- composition examples in `demo/` are the safest templates

## Inventory of plugins in `src/main/scala/vexriscv/plugin`

This section covers the plugin classes found in the source tree. Some are mainstream, some are advanced or specialized.

### Front-end, fetch, and control-flow plugins

| Plugin | Purpose |
| --- | --- |
| `PcManagerSimplePlugin` | Program counter manager. Provides reset vector handling and PC redirection logic. Foundational fetch-side control plugin. |
| `IBusSimplePlugin` | Simple instruction bus frontend without cache. Used in small/minimal CPUs. |
| `IBusCachedPlugin` | Instruction fetch frontend with instruction cache and prediction support. Used in larger/performance-oriented CPUs. |
| `BranchPlugin` | Branch/jump execution and branch prediction integration. Supports multiple prediction modes described in the README. |
| `Fetcher` / `IBusFetcherImpl` | Shared fetch infrastructure used by instruction bus plugins. Not usually instantiated directly by end users, but structurally part of the fetch plugin stack. |

### Decode, register file, source selection, and hazards

| Plugin | Purpose |
| --- | --- |
| `DecoderSimplePlugin` | Instruction decode table and legality handling. Central service provider for many other plugins. |
| `RegFilePlugin` | Integer register file implementation and read timing mode (`SYNC`/`ASYNC` style behavior depending on config). |
| `SrcPlugin` | Generates operand selection signals and source datapath preparation. |
| `HazardSimplePlugin` | Standard hazard detector and bypass controller. One of the most important integration plugins. |
| `HazardPessimisticPlugin` | More conservative hazard handling option. |
| `NoHazardPlugin` | Hazard service stub/no-hazard strategy. |
| `NoPipeliningPlugin` | Specialized control plugin for limiting/altering pipelining behavior. |
| `SingleInstructionLimiterPlugin` | Restricts instruction flow to more serialized behavior for specific use cases/debugging/experimentation. |

### Integer execution plugins

| Plugin | Purpose |
| --- | --- |
| `IntAluPlugin` | Integer ALU operations such as add/sub/logic/compare support. Core datapath plugin. |
| `LightShifterPlugin` | Area-efficient iterative shifter. Used in smaller CPUs. |
| `FullBarrelShifterPlugin` | Single-cycle barrel shifter. Used in higher-performance CPUs. |
| `MulPlugin` | Faster multiplication implementation. |
| `MulSimplePlugin` | Simpler multiplication option. |
| `Mul16Plugin` | Multiplication implementation specialized around 16-bit decomposition/tradeoffs. |
| `DivPlugin` | Divider plugin, implemented as a specialized iterative multiply/divide configuration. |
| `MulDivIterativePlugin` | Iterative multiply/divide engine with configurable generation/unroll factors. |

### Memory, data bus, translation, and protection

| Plugin | Purpose |
| --- | --- |
| `DBusSimplePlugin` | Simple data bus interface without cache. |
| `DBusCachedPlugin` | Cached data bus interface with data cache integration. |
| `MemoryTranslatorPlugin` | Translation infrastructure for virtual memory-style address translation. |
| `StaticMemoryTranslatorPlugin` | Static region-based translation logic for simpler systems. |
| `MmuPlugin` | Full MMU support for Linux/supervisor-capable systems. |
| `PmpPlugin` | Physical Memory Protection support. |
| `PmpPluginNapot` | PMP implementation using NAPOT region handling. |

### CSR, privilege, interrupts, and exceptions

| Plugin | Purpose |
| --- | --- |
| `CsrPlugin` | Main CSR/privilege/interrupt/exception plugin. One of the largest and most central plugins in the project. |
| `MstatushPlugin` | Adds `mstatush` handling support. |
| `ExternalInterruptArrayPlugin` | Expands external interrupt handling to a masked/pending interrupt array exposed through CSRs. |
| `UserInterruptPlugin` | Adds a custom/user-defined interrupt source integrated into the CSR/interrupt model. Defined inside `CsrPlugin.scala`. |
| `HaltOnExceptionPlugin` | Exception-related control plugin that halts on exception conditions. |
| `DummyFencePlugin` | Fence-related compatibility/helper plugin. |

### Debug, reporting, and tooling plugins

| Plugin | Purpose |
| --- | --- |
| `DebugPlugin` | On-core debug module integration, reset behavior, and debug bus connectivity. |
| `EmbeddedRiscvJtag` | Embedded RISC-V JTAG transport/debug integration around the debug subsystem. |
| `YamlPlugin` | Emits a YAML description of the generated CPU, used by tooling such as OpenOCD flows. |
| `FormalPlugin` | Formal verification-oriented instrumentation/support. |

### Floating-point and external functional-unit plugins

| Plugin | Purpose |
| --- | --- |
| `FpuPlugin` | Integrates the floating-point unit into the CPU pipeline. |
| `VfuPlugin` | Generic vector/functional-unit style external execution interface carried through a request/response bus. |
| `CfuPlugin` | Custom Function Unit plugin. Lets instructions dispatch to an external/custom execution unit over a structured bus. |

### Crypto and custom extension plugins

| Plugin | Purpose |
| --- | --- |
| `AesPlugin` | AES-related custom instruction/crypto extension plugin. |

## Which plugins are "required"

The framework itself does not hard-code a single required set, but in practice a usable CPU normally needs a coherent baseline group:

- instruction fetch plugin:
  - `IBusSimplePlugin` or `IBusCachedPlugin`
- data access plugin:
  - `DBusSimplePlugin` or `DBusCachedPlugin`
- decode:
  - `DecoderSimplePlugin`
- register file:
  - `RegFilePlugin`
- source datapath:
  - `SrcPlugin`
- integer execution:
  - `IntAluPlugin`
- hazards:
  - typically `HazardSimplePlugin`
- control flow:
  - `BranchPlugin`
- machine control:
  - usually `CsrPlugin`
- reporting/tooling:
  - often `YamlPlugin`

Then optional features are layered on top:

- `MulPlugin`, `DivPlugin`, `MulDivIterativePlugin`
- `FullBarrelShifterPlugin` or `LightShifterPlugin`
- `DebugPlugin`
- `MmuPlugin`
- `PmpPlugin`
- `FpuPlugin`
- `AesPlugin`
- `CfuPlugin`

## Recommended way to navigate plugin usage in this repo

If you want to understand how plugins are used in practice, read in this order:

1. `src/main/scala/vexriscv/plugin/Plugin.scala`
2. `src/main/scala/vexriscv/Pipeline.scala`
3. `src/main/scala/vexriscv/VexRiscv.scala`
4. `src/main/scala/vexriscv/demo/GenSmallest.scala`
5. `src/main/scala/vexriscv/demo/GenFull.scala`
6. `src/main/scala/vexriscv/demo/CustomInstruction.scala`
7. `src/main/scala/vexriscv/demo/CustomCsrDemoPlugin.scala`

That path shows:

- the plugin interface
- the plugin lifecycle
- how plugins are attached to a CPU
- a minimal composition
- a full composition
- how to create new instruction and CSR extensions

## Summary

The plugin system is the central construction mechanism of VexRiscv.
The CPU core is intentionally thin, while almost all functionality is injected by plugins through:

- a two-phase lifecycle (`setup` then `build`)
- a shared service-discovery model
- stage-to-stage typed signal transport using `Stageable`
- compositional CPU configuration through `VexRiscvConfig.plugins`

If you want to extend this codebase, the safest strategy is:

1. start from an existing `demo/Gen*.scala` configuration,
2. add or remove plugins there,
3. create new features as standalone plugins using the `CustomInstruction` and `CustomCsrDemoPlugin` patterns.
