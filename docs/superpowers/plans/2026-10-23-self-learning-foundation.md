# Self-learning Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Provide local, validated learning samples and a truthful offline training handoff without changing electromagnetic calculations.

**Architecture:** A focused SelfLearningDataset module owns schema, validation, persistence and training bundles. A LearningWorkspace widget provides sample entry/import/export and unavailable-backend/model states inside the existing comparison dialog.

**Tech Stack:** C++17, Qt6 Widgets/Core, JSON, QSaveFile, SHA256; no new runtime dependency.

**Spec:** docs/superpowers/specs/2026-10-23-self-learning-foundation-design.md

## Global Constraints

- Preserve all existing uncommitted work and reuse the scheme-sheet-layout linked worktree.
- No prediction, activation, training-success or accuracy claim without a real validated backend/model.
- No automatic conversion of legacy TestReport zeros; no original engine or optimizer changes.
- Commit and PR remain pending user acceptance. Deployment uses existing desktop shortcut target with recoverable backup.

### Task 1: Dataset contract and persistence

**Files:** Create `qt-app/src/core/SelfLearningDataset.h`, `.cpp`, `qt-app/tests/self_learning_test.cpp`; modify `qt-app/CMakeLists.txt`.

**Interfaces:** `SelfLearning::Sample`, `makeSample(CalcInput,Sample&,QString&)`, `encode(Sample)`, `decode(QJsonObject,Sample&,QString&)`, `load(QString,QVector<Sample>&,QString&)`, `save(QString,QVector<Sample>,QString&)`, `importDataset(QString,QByteArray,QString&)`, `trainingBundle(QVector<Sample>,QJsonObject&,QString&)`.

- [x] Write tests with literal labels: baseline524W, measured530W -> residual6W; missing other labels stay null. Verify missing/invalid input, unsupported shape, unknown version, duplicate/conflicting IDs and damaged stores are rejected without changing files.
- [x] Configure/build `test-self-learning`; observe missing functionality fail before implementation.
- [x] Implement strict JSON v1 schema, complete snapshot roundtrip, engine baseline verification, optional metrics, atomic save/merge; tests use temporary directories only.
- [x] Run `ctest -R self-learning-foundation --output-on-failure` and existing core-selection test.

### Task 2: GUI workflow and model-status seam

**Files:** Create `qt-app/src/gui/widgets/LearningWorkspace.h`, `.cpp`; modify `SelfLearnDialog.h`, `.cpp`, `EnterCalcPage.cpp`; extend test target.

**Interfaces:** `LearningWorkspace(QString storagePath,QWidget*)`, `setScheme(CalcInput,QString)`; consumes the dataset API above. `trainingBlockReason(QVector<Sample>)` supplies explicit empty-data/backend-unavailable statuses; model correction remains unavailable.

- [x] Test empty workspace: no save without selected scheme/product ID, export requires measured labels, train/activate/rollback disabled, corrected values absent. Use named widgets and real UI button clicks, no source-text assertions.
- [x] Run tests to observe missing widget/behavior failure.
- [x] Build three tabs: original comparison, samples (new from selected scheme, metadata and optional labels, load/edit, JSON template/import/export), training/model status (per-target counts, training bundle export, backend/model notices, disabled deployment controls).
- [x] Save edits transactionally; require explicit user action for draft creation and do not silently discard unsaved input on scheme changes. Display read/validation/write errors.
- [x] Run tests, production build, independent review, and offscreen visual capture. Fix actionable findings with regression tests.

### Task 3: Handoff and deployment

**Files:** Create `docs/SELF_LEARNING_FOUNDATION_ACCEPTANCE.md`.

- [x] Document template fields, storage location, sample flow, partial measurements, no-backend limitations and acceptance checks.
- [x] Run fresh tests and verify executable build. If ZTBLD is running, request closure without terminating it.
- [x] Back up the current desktop target and replace only after successful checks; verify source/target SHA256 equality. Do not commit pending user acceptance.

## Execution record

- Baseline core-selection-state: passed before changes (2026-10-23).
- Design approved in chat; user requested first batch. Implementation proceeds inline because dataset and UI contracts are tightly coupled; independent reviewer follows build.
- Independent review verified data-race/size/precision/discard/group/schema/duplicate risks; each was corrected with regression tests. Imports and GUI updates use one inter-process-locked transaction.
- Full production build and all3 CTest cases passed on2026-10-23 (1.78s total). Existing calculation-page test was stale: adapted finished signal signature, explicit confirmed criteria, isolated QSettings INI user/system paths, and modal diagnostic guard/25s timeout. No original engine/optimizer logic changed in this batch.
- Source and visual QA: learning sample and training pages captured offscreen with existing application stylesheet; untrained/disabled states explicit.
- Final fresh build and3/3 CTest cases passed (1.85s); independent focused re-review reported no outstanding actionable issues.
- Deployed to desktop shortcut's existing executable after confirming ZTBLD was not running. SHA256 CE564B2908DC5A612EE53982F7B6996CEFBBAE02AF0582BB59C806E8E1172670 (3457510bytes).
- Recoverable backup: `D:/zt-transformer/zt-transformer-ai-main/build/ZTBLD-before-self-learning-foundation-20261023.exe`; old hash55955C2E7C58C2DA283441EE7F2492943A091C3E722A74C53C08349955C69D14. Shortcut unchanged. Changes remain uncommitted pending acceptance.
