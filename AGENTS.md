# Agent Guidelines for TouchFreeze

## Development & Build Workflow
- **Build Command**: Use `powershell -ExecutionPolicy Bypass -File .\build.ps1` for building and verifying the executable, hook DLL, and MSI installer.
- **Release Command**: Use `powershell -ExecutionPolicy Bypass -File .\release.ps1` (or `powershell -ExecutionPolicy Bypass -File .\release.ps1 -Version X.X.X -Message "..."`) for a complete 1-command release (version bump, local build, git commit & tag, push, CI/CD monitoring, and release verification).
- **Command Consolidation**: Always prefer consolidating multi-step tasks into dedicated PowerShell scripts rather than executing granular commands that require separate interactive approvals.
- **Continuous Optimization**: Regularly review repeated actions during task execution and integrate them into reusable build/test scripts.
