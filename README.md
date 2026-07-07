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
| `ADC0_MEMRES0` | `0x40001280` | ADC result memory 0 |
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

Difficulties:

- The system-level `cmd.exe` AutoRun prints `Active code page: 65001`, which CCS can misinterpret as a generated file.
- Generated files under `Debug/` are volatile; manual edits there are overwritten by CCS.
- SysConfig initializes ADC/DMA registers, but application code still has to set DMA source/destination addresses and enable/start the transfer.

Next step:

- Fix the ADC trigger path so ADC0 produces DMA trigger events. Current evidence indicates DMA channel setup is present, but ADC DMA raw status remains zero and no samples are copied into `gADCSamples`.

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
