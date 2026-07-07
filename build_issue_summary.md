# CCS Theia 构建问题总结

## 1. 问题现象

在 CCS Theia 中构建 `cmsis_dsp_empty_LP_MSPM0G3507_nortos_ticlang` 项目时，SysConfig 可以正常运行并生成代码，但后续编译阶段失败。

典型日志如下：

```text
gmake: *** No rule to make target 'cmsis_dsp_empty.o', needed by 'all'.
cc    -c -o ti_msp_dl_config.o ti_msp_dl_config.c
fatal error: ti/devices/msp/msp.h: No such file or directory
```

关键异常点是构建过程使用了系统默认的 `cc` 编译器，而不是 TI ARM 编译器。

正常情况下，日志中应出现：

```text
Invoking: Arm Compiler
D:/diansaiapp/CCS/ccs/tools/compiler/ti-cgt-armllvm_4.0.0.LTS/bin/tiarmclang.exe
```

## 2. 问题成因

Windows 系统中存在如下 `cmd.exe` 自动启动项：

```text
HKLM\Software\Microsoft\Command Processor\AutoRun = chcp 65001
```

该设置会导致每次启动 `cmd.exe` 时输出：

```text
Active code page: 65001
```

CCS Theia 构建工程时会调用 SysConfig，而 SysConfig 的调用过程经过 `cmd.exe`。因此这行 `Active code page: 65001` 输出混入了 CCS managed build 的生成流程。

CCS 将其中的 `65001` 误识别为一个生成文件，并写入自动生成的 make 文件：

```text
Debug/subdir_vars.mk
Debug/subdir_rules.mk
```

污染后的 make 文件中会出现类似内容：

```make
GEN_MISC_FILES += \
./\ 65001 \
./device.cmd.genlibs \
./ti_msp_dl_config.h \
./Event.dot
```

以及：

```make
\ 65001: build-224171775 ../cmsis_dsp_empty.syscfg
```

这些异常依赖会破坏 GNU make 的规则匹配，导致 TI 的编译规则没有被正确使用。最终 make 回退到内置默认规则，调用系统默认的 `cc` 编译 `ti_msp_dl_config.c`，从而出现找不到 MSPM0 SDK 头文件的问题。

## 3. 当前项目级解决方法

本项目已添加项目级 workaround：

```text
makefile.init
```

该文件位于项目根目录：

```text
C:\Users\carpediem\workspace_ccstheia\cmsis_dsp_empty_LP_MSPM0G3507_nortos_ticlang\makefile.init
```

其作用是在 GNU make 启动时自动创建 `Debug/65001` 占位文件，使被误写入的 `65001` 依赖可以被满足。

这样即使 CCS 后续再次生成 `Debug/subdir_vars.mk` 和 `Debug/subdir_rules.mk`，构建也不会回退到默认 `cc`，而是继续使用 TI 编译器：

```text
D:/diansaiapp/CCS/ccs/tools/compiler/ti-cgt-armllvm_4.0.0.LTS/bin/tiarmclang.exe
```

## 4. 验证结果

使用 CCS 同款构建命令验证通过：

```powershell
D:\diansaiapp\CCS\ccs\utils\bin\gmake.exe -k -j 32 all -O
```

成功日志中可以看到：

```text
Invoking: Arm Compiler
tiarmclang.exe
```

以及链接成功：

```text
Building target: "cmsis_dsp_empty_LP_MSPM0G3507_nortos_ticlang.out"
Finished building target: "cmsis_dsp_empty_LP_MSPM0G3507_nortos_ticlang.out"
```

最终输出文件为：

```text
Debug/cmsis_dsp_empty_LP_MSPM0G3507_nortos_ticlang.out
```

## 5. 更彻底的系统级修复方法

更彻底的修复方式是修改系统级 `cmd.exe` AutoRun，使 `chcp 65001` 静默执行。

使用管理员权限运行：

```cmd
reg add "HKLM\Software\Microsoft\Command Processor" /v AutoRun /t REG_SZ /d "chcp 65001 >nul" /f
```

该命令不会取消 UTF-8 代码页设置，只是禁止输出：

```text
Active code page: 65001
```

这样可以从根源避免 CCS 将 `65001` 误识别为生成文件。

当前环境没有管理员权限，因此采用了项目级 `makefile.init` workaround。
