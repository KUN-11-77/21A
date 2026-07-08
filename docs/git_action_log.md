# Git Action Log

## 2026-07-08 14:30 +08:00

1. Verified repository state with `git rev-parse --is-inside-work-tree`; the project was already a Git work tree, so `git init` was not run again.
2. Checked current branch and remote configuration with `git branch --show-current` and `git remote -v`; current branch is `master`, and no remote is configured.
3. Reviewed working tree state with `git status --short`; current firmware/source changes are in application source, SysConfig source, and README files.
4. Verified the current firmware build with `D:\diansaiapp\CCS\ccs\utils\bin\gmake.exe -C Debug all`.
5. Verified the current flashed/readback version before committing: 1 kHz square-wave input reported `gDebugPeakFreqHz`, `gDebugBaseFreqHz`, and `gDebugTimeFreqHz` at approximately `999.99963 Hz`.
6. Prepared this action log so the repository records the initialization, commit, and push workflow steps.

## 2026-07-08 14:45 +08:00

1. Updated `TIMER_0` sampling from the previous low-rate setup to `LOAD=14`, giving `32 MHz / 15 = 2.133333 MS/s`.
2. Extended the FFT automatic-base search limit to `100 kHz` while keeping H1-H10 harmonic tracking and neighborhood-energy THD calculation.
3. Regenerated and rebuilt the CCS/SysConfig output with `D:\diansaiapp\CCS\ccs\utils\bin\gmake.exe -C Debug all`; SysConfig reported `TIMER_0_INST_LOAD_VALUE = 14U` and the build completed successfully.
4. Reviewed the 100 kHz square-wave watch result; frequency tracking, odd harmonic bins, and THD around 41.5% were accepted for this high-speed firmware version.

## 2026-07-08 15:10 +08:00

1. Added adaptive sample-mode selection: each hardware frame starts in high-speed mode, then retries in low-speed mode only when the coarse detected frequency is below the configured threshold.
2. Set `APP_ADAPTIVE_LOW_FREQ_THRESHOLD_HZ` to `4000 Hz`, keeping 1 kHz and 3 kHz on the low-speed path while preserving high-speed harmonic coverage for 5 kHz through 100 kHz.
3. Verified the adaptive build with `D:\diansaiapp\CCS\ccs\utils\bin\gmake.exe -C Debug all`; the generated `.out` was up to date before committing and pushing.
4. Prepared to push the current repository version to `https://github.com/R1ezzTa/21A.git`.
