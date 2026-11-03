# Candidate DNN comparison implementation plan

**Goal:** Batch independent DNN corrections for already calculated visible candidates, without mutating any optimizer, formal ranking, CalcResult, quotation, print or selection state.

**Approved design:** Fifth-batch scope approved in chat on 2026-10-24. Read-only result extension using existing residual inference; not an acceleration surrogate and not a feasibility evaluator. Independent display approval remains display-only; experiments/test models remain visibly experimental/test, never promoted.

**Architecture:** EnterCalcPage copies visible rows in their existing display order into a modal comparison dialog. Core builds an immutable bounded candidate snapshot and validates every output binding. Python batch prediction reuses model loading and target-specific applicability checks. Local UUID job stores input, report and completion state separately from samples/models.

**Stack:** Existing Qt6/C++17, QProcess and CPU PyTorch; no new dependencies or network.

## Global constraints

- Preserve all existing dirty work in the linked workspace; no source commit/push before user acceptance.
- No original candidate is changed, reordered, filtered, selected or sent to quotation/printing by this feature.
- Model absence and every target failure remain visible; never substitute zero or old values. No error/accuracy claims without measured labels.
- Compare exactly the current visible candidate snapshot; show its count, retained/hidden context, original list order and immutable model version. No silent clipping; reject snapshots above16MiB.
- Every launch and completion rechecks model/report SHA256; approved-default operations also bind/recheck registry snapshot. Model switch/cancel/failure invalidates displayed corrections and saves no successful completion.
- Tests use temporary data only. CPU runtime and deployment paths follow the preceding four batches.

## 1. Batch backend (tests first)

Add `predict-batch --model PATH --input PATH --output FRESH_UUID_DIR [--cancel-file PATH]`. Reuse safe JSON model/report loading and cached per-target networks; do not train. Input root format `ZTBLD-CandidateComparisonInput`, version1, usage `independent-comparison-only`, runId, candidates array.

Each candidate has candidateId, schemeId positive integer, originalOrder1-based, materialCost, costBasis text and encoded SelfLearning sample (actual input and six original baseline metrics; measured allnull). All candidates remain in output even if all targets fail.

Output `comparison.json`: format `ZTBLD-CandidateComparison`, version1, usage as input, status completed, runId, inputPath/inputSha256, modelPath/modelSha256/reportSha256/modelVersion/scope/engineVersion/testOnly/productionEligible=false. Each output candidate copies candidateId/schemeId/originalOrder/materialCost/costBasis and baseline; predictions has exactly six metric keys, null for failure; failures maps failed metric keys to readable reasons. Target-specific failures must not hide successful other targets. Record counts candidates/successfulTargets/failedTargets. No labels or accuracy/improvement estimates. Atomic publish to new UUID directory; rehash evidence before publication; cancellation must not publish.

Tests: real temporary trained model; complete/partial/missing-target/context/bounds failures, duplicate identifiers/order, invalid baselines, allnull measured, zero corrected temperature, testflags, cancellation/no overwrite/tampering and original ordering. Observe missing-feature RED before implementing, then real CPU GREEN.

## 2. Core snapshot/record integrity (tests first)

New LearningCandidateComparison.h/.cpp: `QJsonObject makeInput(const QVector<OptimizeCandidate>&,QString& error)` and `bool validateReport(const QJsonObject& input,const QJsonObject& report,const QString& modelPath,const QString& inputPath,QString&error)`.

Preserve source candidates and exact cost/baselines. Validate IDs/order/finite data/actual original result validity, full CalcInput serialization and16MiB limit; use sample metadata for independent comparison only, never training samples. Unsupported model physical context is reported per candidate by the backend, not filtered from the snapshot. Report validator binds exact input file/content, all original rows/order/baseline/cost, current model/report digests/version/testflags/scope, complete prediction/failure coverage and summary counts. No artifact is automatically approved.

## 3. Dialog and result-page integration (tests first)

New CandidateComparisonDialog uses QProcess and existing runtime configuration; chooses approved default or explicit completed experimental/test version. Separate original/corrected values and per-target reasons, cancellation, progress and local saved-record path. Freeze comparison job input before launching; finalize success only after core integrity validation and current approval/model bindings. Display all six targets without sorting original formal data. Close cancels own child only. No model/runtime means clear blocked state.

EnterCalcPage button “DNN旁路对比” opens only outside active optimizer runs and takes copies of nonhidden SchemeTableWidget rows via schemeIdx mapping. Missing mapping rejects whole operation. No writeback signals. Regression proves row order/values/formal print and result state unchanged after opening/closing.

## 4. Verification/deployment

Build and run all existing8 C++ tests plus new comparison core/dialog tests and full Python suite. Independent read-only review of feature and failure paths. Inspect offscreen screenshot. After user closes desktop app (or read-only process check confirms none), retain old EXE/backend backup and atomically replace same desktop-shortcut target; compare SHA256. Document entry/limits/checklist, then wait user acceptance. Do not claim actual DNN engineering accuracy or optimizer acceleration.
