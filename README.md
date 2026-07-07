## Example Summary
Empty project using CMSIS DSP.
This example shows a basic empty project using CMSIS DSP with just main file
and SysConfig initialization.


## Peripherals & Pin Assignments

| Peripheral | Pin | Function |
| --- | --- | --- |
| SYSCTL |  |  |
| DEBUGSS | PA20 | Debug Clock |
| DEBUGSS | PA19 | Debug Data In Out |

## BoosterPacks, Board Resources & Jumper Settings

Visit [LP_MSPM0G3507](https://www.ti.com/tool/LP-MSPM0G3507) for LaunchPad information, including user guide and hardware files.

| Pin | Peripheral | Function | LaunchPad Pin | LaunchPad Settings |
| --- | --- | --- | --- | --- |
| PA20 | DEBUGSS | SWCLK | N/A | <ul><li>PA20 is used by SWD during debugging<br><ul><li>`J101 15:16 ON` Connect to XDS-110 SWCLK while debugging<br><li>`J101 15:16 OFF` Disconnect from XDS-110 SWCLK if using pin in application</ul></ul> |
| PA19 | DEBUGSS | SWDIO | N/A | <ul><li>PA19 is used by SWD during debugging<br><ul><li>`J101 13:14 ON` Connect to XDS-110 SWDIO while debugging<br><li>`J101 13:14 OFF` Disconnect from XDS-110 SWDIO if using pin in application</ul></ul> |

### Device Migration Recommendations
This project was developed for a superset device included in the LP_MSPM0G3507 LaunchPad. Please
visit the [CCS User's Guide](https://software-dl.ti.com/msp430/esd/MSPM0-SDK/latest/docs/english/tools/ccs_ide_guide/doc_guide/doc_guide-srcs/ccs_ide_guide.html#sysconfig-project-migration)
for information about migrating to other MSPM0 devices.

### Low-Power Recommendations
TI recommends to terminate unused pins by setting the corresponding functions to
GPIO and configure the pins to output low or input with internal
pullup/pulldown resistor.

SysConfig allows developers to easily configure unused pins by selecting **Board**→**Configure Unused Pins**.

For more information about jumper configuration to achieve low-power using the
MSPM0 LaunchPad, please visit the [LP-MSPM0G3507 User's Guide](https://www.ti.com/lit/slau873).

## Example Usage
Compile and load the example.

## Project Debug Notes

### Environment

- IDE/toolchain: CCS Theia under `D:\diansaiapp\CCS`
- Compiler: TI Arm Clang `ti-cgt-armllvm_4.0.0.LTS`
- SDK: MSPM0 SDK `2.10.00.04`
- Device: `MSPM0G3507`
- Debug probe: XDS110, using [targetConfigs/MSPM0G3507.ccxml](targetConfigs/MSPM0G3507.ccxml)
- Main output: `Debug/cmsis_dsp_empty_LP_MSPM0G3507_nortos_ticlang.out`

### Build Method

Build from the `Debug` directory with the same command CCS Theia uses:

```powershell
D:\diansaiapp\CCS\ccs\utils\bin\gmake.exe -k -j 32 all -O
```

Successful builds must show `tiarmclang.exe` as the compiler:

```text
Invoking: Arm Compiler
D:/diansaiapp/CCS/ccs/tools/compiler/ti-cgt-armllvm_4.0.0.LTS/bin/tiarmclang.exe
```

If the log shows the generic command below, the build has fallen back to the wrong compiler:

```text
cc -c -o ti_msp_dl_config.o ti_msp_dl_config.c
```

### Build Issue and Workaround

The host has a system-level `cmd.exe` AutoRun entry:

```text
HKLM\Software\Microsoft\Command Processor\AutoRun = chcp 65001
```

That command prints `Active code page: 65001` every time `cmd.exe` starts. CCS/SysConfig can mis-detect the `65001` token as a generated file and inject it into `Debug/subdir_vars.mk` and `Debug/subdir_rules.mk`, which can break make rule selection and make GNU make fall back to the generic `cc` compiler.

The project-level workaround is [makefile.init](makefile.init). CCS includes this file automatically from `Debug/makefile`; it creates a `Debug/65001` marker file so the polluted generated dependency is satisfied and the TI compiler rule remains usable.

A cleaner system-level fix is to run this from an elevated command prompt:

```cmd
reg add "HKLM\Software\Microsoft\Command Processor" /v AutoRun /t REG_SZ /d "chcp 65001 >nul" /f
```

This preserves UTF-8 code page setup but suppresses the printed line that confuses CCS.

### Flash Method

Program and verify the current `.out` with DSLite:

```powershell
D:\diansaiapp\CCS\ccs\ccs_base\DebugServer\bin\DSLite.exe flash `
  --config=targetConfigs\MSPM0G3507.ccxml `
  --flash --verify --run --verbose `
  Debug\cmsis_dsp_empty_LP_MSPM0G3507_nortos_ticlang.out
```

Successful flashing ends with:

```text
Program verification successful
Running...
Success
```

### ADC + DMA Debug Method

The current application configures:

- ADC0, channel 0, input pin `PA27`
- DMA channel 0
- 1024 half-word samples into `gADCSamples`

Useful runtime checks:

- Set a breakpoint at `__BKPT(0)` in [cmsis_dsp_empty.c](cmsis_dsp_empty.c). Reaching it means the DMA-done path set `gADCDMADone = true`.
- Watch `gADCSamples[0..1023]` in CCS Expressions/Memory Browser.
- Watch `gADCDMADone`; it should become `true` after the DMA completion interrupt.

DSLite can also read hardware registers directly. Examples:

```powershell
D:\diansaiapp\CCS\ccs\ccs_base\DebugServer\bin\DSLite.exe memory `
  --config=targetConfigs\MSPM0G3507.ccxml `
  --range=0x4042B200,4 --size=32 `
  --output=Debug\regdump\DMA_CH0_DMACTL.bin
```

Important addresses used during inspection:

| Register | Address | Purpose |
| --- | --- | --- |
| `ADC0_CTL0` | `0x40001100` | ADC control |
| `ADC0_DMA_IMASK` | `0x40001088` | ADC DMA trigger mask |
| `ADC0_DMA_RIS` | `0x40001090` | ADC DMA raw event status |
| `ADC0_MEMRES0` | `0x40001280` | ADC result memory 0 register view |
| `ADC0_MEMRES0_SVT` | `0x40556280` | ADC result memory 0 SVT address returned by `DL_ADC12_getMemResultAddress()` and used as DMA source |
| `DMA_CH0_DMACTL` | `0x4042B200` | DMA channel 0 control |
| `DMA_CH0_DMASA` | `0x4042B204` | DMA source address |
| `DMA_CH0_DMADA` | `0x4042B208` | DMA destination address |
| `DMA_CH0_DMASZ` | `0x4042B20C` | DMA transfer size |
| `TIMA0_CTR` | `0x40861800` | TIMA0 counter |

## Project Progress

### 2026-07-07

Purpose: Bring up a CCS Theia MSPM0G3507 CMSIS-DSP project, confirm the toolchain, flash the board, and start ADC+DMA validation.

Progress:

- Confirmed the project was initially not a Git repository.
- Fixed the CCS build failure where SysConfig/code-page output caused make to use the wrong `cc` compiler.
- Added [makefile.init](makefile.init) as a project-local workaround for the `65001` generated dependency issue.
- Built successfully with TI Arm Clang.
- Flashed and verified `Debug/cmsis_dsp_empty_LP_MSPM0G3507_nortos_ticlang.out` through XDS110 using DSLite.
- Added ADC+DMA runtime logic in [cmsis_dsp_empty.c](cmsis_dsp_empty.c): sample buffer, DMA source/destination setup, DMA enable, ADC start, and DMA-done interrupt flag.
- Read ADC/DMA/TIMA0 registers through DSLite to inspect runtime state.
- Rebuilt the current ADC+DMA application successfully with TI Arm Clang.
- Flashed and ran the ADC+DMA application again. Flash verify and `Running... Success` passed; runtime readback showed DMA channel source/destination/enable registers configured, but `gADCDMADone` remained `false` and the first ADC sample buffer values were still zero.
- Rebuilt and flashed the version that starts `TIMER_0_INST`. Runtime readback confirmed TIMA0 is now counting, but ADC DMA raw status is still zero and `gADCDMADone` is still false.
- Read the current progress notes and inspected the working-tree diff. The active local test version reconfigures ADC0 to software-triggered repeat-single conversion so ADC+DMA can be debugged separately from the TIMER event path.
- Ran the documented CCS Theia build command from `Debug/`; `gmake` reported the current `.out` was already up to date for this local test version.
- Flashed the software-triggered ADC+DMA isolation firmware. Flash verify/run succeeded, but runtime readback showed `gADCDMADone = 0`, the first `gADCSamples` words were still zero, `ADC0_CPU_INT_RIS = 0x00000002` (`TOVIFG`), `ADC0_CPU_INT_MIS = 0`, `ADC0_DMA_TRIG_RIS = 0`, `ADC0_CTL1 = 0x00120100`, `ADC0_CTL2 = 0x00000900`, and `ADC0_STATUS = 0x1`.
- Compared against TI SDK ADC DMA examples under `D:\diansaiapp\CCS\mspm0_sdk_2_10_00_04\examples\nortos\LP_MSPM0G3507`. The examples use software trigger with ADC auto sampling and DMA `DL_DMA_FULL_CH_REPEAT_SINGLE_TRANSFER_MODE`, while this project had been using manual sampling plus generated `DL_DMA_SINGLE_TRANSFER_MODE`.
- Updated [cmsis_dsp_empty.c](cmsis_dsp_empty.c) to match the TI example pattern for the isolation test: repeat-single ADC with auto sampling, explicit DMA channel disable before setup, DMA repeat-single transfer mode, cleared ADC DMA_DONE status, and explicit `DL_ADC12_enableDMA()` before starting conversion.
- Rebuilt the patched ADC+DMA isolation firmware successfully with TI Arm Clang.
- Flashed and verified the patched ADC+DMA isolation firmware. Runtime readback confirmed `gADCDMADone = 1`, `ADC0_CPU_INT_IIDX = 0x00000006` (`DMA_DONE`), `ADC0_CPU_INT_MIS = 0x00000020`, and the first sample buffer words were non-zero ADC values such as `0x0233`, `0x022F`, `0x0234`, and `0x0236`.
- For FFT input, switched the runtime path back from software-triggered ADC isolation to timer-paced sampling: `TIMER_0` zero event publishes on event channel 1, `ADC12_0` subscribes on channel 1, ADC0 performs one conversion per timer event, and DMA channel 0 captures 1024 half-word samples before ADC DMA_DONE.
- Rebuilt the timer-event ADC+DMA firmware successfully with TI Arm Clang.
- Flashed the first timer-event ADC+DMA firmware. Runtime readback confirmed the event path itself was now connected (`TIMA0_FPUB0 = 1`, `TIMA0_GEN_EVENT0_IMASK = 1`, `ADC0_FSUB0 = 1`) and DMA advanced by one half-word (`DMADA = 0x20200002`, `DMASZ = 0x000003FF`), but `gADCDMADone` stayed 0. This showed ADC event triggering worked once, but ADC was not staying armed for repeated timer events.
- Updated ADC0 to `DL_ADC12_REPEAT_MODE_ENABLED` while keeping `DL_ADC12_TRIG_SRC_EVENT`, so repeated `TIMER_0` zero events can fill the full 1024-sample DMA block.
- Rebuilt the repeat event-triggered ADC+DMA firmware successfully with TI Arm Clang.
- Flashed and checked the repeat event-triggered firmware. `gADCDMADone = 1`, ADC CPU interrupt index was `0x00000006` (`DMA_DONE`), and both the beginning and end of `gADCSamples` contained non-zero ADC values. However, DMA registers showed the channel had already started a later pass through the buffer (`DMADA = 0x20200476`, `DMASZ = 0x0000014E`), so the timer needed to be stopped at DMA_DONE to freeze an FFT frame.
- Updated the ADC DMA_DONE ISR to stop `TIMER_0`, disable DMA channel 0, and disable ADC DMA before setting `gADCDMADone`, so the 1024-sample frame remains stable for FFT processing.
- Rebuilt the DMA_DONE frame-freeze firmware successfully with TI Arm Clang.
- Flashed and checked the frame-freeze firmware. It still showed a few samples of next-frame overwrite before the ISR disabled the peripherals (`DMADA = 0x20200008`, `DMASZ = 0x000003FC`). DriverLib documentation clarified that `DL_DMA_SINGLE_TRANSFER_MODE` is the correct mode for "one data transfer per DMA trigger, `DMASZ` triggers until block done"; the earlier one-sample behavior came from ADC non-repeat mode, not DMA single mode.
- Updated DMA channel 0 to `DL_DMA_SINGLE_TRANSFER_MODE` while keeping ADC repeat event mode and the DMA_DONE frame-freeze ISR.
- Rebuilt the DMA single-transfer + ADC repeat-event firmware successfully with TI Arm Clang.
- Flashed and verified the final timer-event ADC+DMA capture firmware. Runtime readback confirmed `gADCDMADone = 1`, `TIMA0_FPUB0 = 1`, `ADC0_FSUB0 = 1`, `DMADA = 0x20200800`, `DMASZ = 0`, and both the beginning and end of `gADCSamples` contained non-zero ADC data. This confirms `TIMER_0` event-triggered ADC sampling filled exactly one 1024-point DMA frame and stopped without starting the next frame.

Difficulties:

- The system-level `cmd.exe` AutoRun prints `Active code page: 65001`, which CCS can misinterpret as a generated file.
- Generated files under `Debug/` are volatile; manual edits there are overwritten by CCS.
- SysConfig initializes ADC/DMA registers, but application code still has to set DMA source/destination addresses and enable/start the transfer.

Next step:

- ADC+DMA is validated with `TIMER_0` event-triggered sampling and a frozen 1024-sample frame. Next, add the CMSIS-DSP FFT processing path over `gADCSamples` after `gADCDMADone` becomes true.

## Action Log

| Time | Action | Result |
| --- | --- | --- |
| 2026-07-07 | Checked `git rev-parse --is-inside-work-tree` | Directory was not a git repository. |
| 2026-07-07 | Investigated CCS build failure | Found generated makefile pollution from `65001`. |
| 2026-07-07 | Added `makefile.init` workaround | CCS builds continue using `tiarmclang.exe`. |
| 2026-07-07 | Built project with `gmake -k -j 32 all -O` | Build succeeded and produced `.out`. |
| 2026-07-07 | Flashed with DSLite | Program load and verification succeeded. |
| 2026-07-07 | Read ADC/DMA registers with DSLite | Confirmed configuration visibility and identified runtime start requirements. |
| 2026-07-07 | Added project README notes and `.gitignore` | Repository documentation prepared before git initialization. |
| 2026-07-07 | Rebuilt current ADC+DMA application | Build and link succeeded with `tiarmclang.exe`. |
| 2026-07-07 | Initialized Git repository | Created local git history and prepared initial tracked file set. |
| 2026-07-07 | Flashed and checked current ADC+DMA firmware | Flash/verify/run succeeded; DMA registers were configured, but ADC DMA completion did not occur. |
| 2026-07-07 | Flashed TIMER-start firmware and checked runtime state | Flash/verify/run succeeded; TIMA0 counter advanced, but ADC DMA trigger/completion still did not occur. |
| 2026-07-07 15:03 | Read current progress and checked local diff | Confirmed the active working-tree firmware isolates ADC+DMA with software-triggered repeat-single ADC conversion. |
| 2026-07-07 15:03 | Ran documented build command | `gmake` reported `cmsis_dsp_empty_LP_MSPM0G3507_nortos_ticlang.out` is up to date. |
| 2026-07-07 15:04 | Flashed software-triggered ADC+DMA isolation firmware | Flash/verify/run succeeded, but runtime state showed ADC `TOVIFG`, no ADC DMA_DONE, no DMA trigger status, and an unchanged zero sample buffer. |
| 2026-07-07 15:26 | Compared against TI SDK ADC DMA examples and patched firmware | Switched the isolation test toward the TI pattern: ADC auto sampling, DMA repeat-single transfer mode, clear DMA_DONE, and re-enable ADC DMA before start. |
| 2026-07-07 15:29 | Rebuilt patched ADC+DMA isolation firmware | Build succeeded with `tiarmclang.exe` and regenerated `Debug/cmsis_dsp_empty_LP_MSPM0G3507_nortos_ticlang.out`. |
| 2026-07-07 15:32 | Flashed and checked patched ADC+DMA isolation firmware | Flash/verify/run succeeded; `gADCDMADone` became 1, ADC CPU interrupt index reported DMA_DONE, and `gADCSamples` contained non-zero ADC data. |
| 2026-07-07 15:59 | Switched runtime path back to TIMER event-triggered ADC for FFT sampling | Patched [cmsis_dsp_empty.c](cmsis_dsp_empty.c) so `TIMER_0` publishes zero events on channel 1, ADC0 subscribes on channel 1, and DMA still captures 1024 samples before ADC DMA_DONE. |
| 2026-07-07 15:59 | Rebuilt timer-event ADC+DMA firmware | Build succeeded with `tiarmclang.exe` and regenerated the `.out` for flashing. |
| 2026-07-07 16:03 | Flashed and checked first timer-event ADC+DMA firmware | Event channel setup was correct and one sample transfer occurred, but ADC was not configured for repeated event-triggered conversions, so DMA did not reach 1024 samples. |
| 2026-07-07 16:03 | Enabled repeated event-triggered ADC conversions | Changed ADC0 from single event conversion to repeat event conversion so the timer can drive the full 1024-sample DMA capture. |
| 2026-07-07 16:04 | Rebuilt repeat event-triggered ADC+DMA firmware | Build succeeded with `tiarmclang.exe`. |
| 2026-07-07 16:07 | Flashed and checked repeat event-triggered ADC+DMA firmware | DMA_DONE occurred and the buffer contained non-zero samples, but the timer kept running after DMA_DONE and DMA began overwriting the next frame. |
| 2026-07-07 16:07 | Added DMA_DONE frame freeze logic | ADC DMA_DONE ISR now stops `TIMER_0`, disables DMA channel 0, disables ADC DMA, and then sets `gADCDMADone`. |
| 2026-07-07 16:08 | Rebuilt DMA_DONE frame-freeze firmware | Build succeeded with `tiarmclang.exe`. |
| 2026-07-07 16:11 | Checked frame-freeze firmware and DMA mode semantics | DMA_DONE occurred, but repeat-single DMA mode could start a next frame before ISR shutdown; decided to use `DL_DMA_SINGLE_TRANSFER_MODE` with ADC repeat event mode. |
| 2026-07-07 16:11 | Switched DMA channel 0 to single transfer mode | Kept one half-word per ADC DMA trigger and 1024 triggers per frame, but removed automatic full-channel repeat. |
| 2026-07-07 16:12 | Rebuilt DMA single-transfer + ADC repeat-event firmware | Build succeeded with `tiarmclang.exe`. |
| 2026-07-07 16:14 | Flashed and verified final timer-event ADC+DMA capture firmware | `gADCDMADone` became 1, DMA stopped at `DMADA = 0x20200800` with `DMASZ = 0`, and the 1024-sample buffer contained non-zero data at both the beginning and end. |
