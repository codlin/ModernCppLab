## Related Issue

Closes #

## Summary

-

## Changes

-

## Affected Areas

- [ ] `include/`（头文件）
- [ ] `src/`（源文件）
- [ ] `CMakeLists.txt` / `CMakePresets.json`（构建系统）
- [ ] `docs/`（文档）
- [ ] `tests/`（如存在）
- [ ] `.github/`（CI / 模板 / 流程）
- [ ] `CLAUDE.md` / `AGENTS.md`

## Documentation Read

- [ ] `docs/github-mcp-workflow.md`
- [ ] Related architecture docs:
      -
- [ ] Related planning docs:
      -
- [ ] Related review docs:
      -
- [ ] Related knowledge/discussion docs:
      -

## Verification

- [ ] Build（`cmake --preset default && cmake --build cmake-build-default`）
- [ ] Unit tests（`ctest --test-dir cmake-build-default`，如已配置）
- [ ] Manual run（`./cmake-build-default/ModernCppLab`）
- [ ] No compiler warnings introduced
- [ ] Not required, reason:

## Safety / Compatibility

- [ ] No breaking API change（public header unchanged）
- [ ] No ABI break（C++ standard / compiler compatibility）
- [ ] No data file format change（`data/tasks.json` compatible）
- [ ] No change to CMake minimum version or C++ standard
- [ ] No new raw owning pointers; RAII preserved
- [ ] Exception safety considered
- [ ] Not applicable, reason:

## Documentation Update

- [ ] 不需要更新文档
- [ ] 已更新 `docs/...`
- [ ] 已更新 `README.md`
- [ ] 已更新 `CLAUDE.md`
- [ ] 已更新 `AGENTS.md`

## Resume Note

如果这个 PR 没合并，下次继续时先看：

-
