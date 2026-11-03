# Experimental local CPU DNN backend

Use a dedicated Python runtime with NumPy and CPU PyTorch (requirements.txt).
Dependency installation belongs to runtime setup, never the trainer. No GPU required.

```
python trainer.py probe
python trainer.py train --bundle bundle.json --output new-job --epochs 200 --cancel-file cancel.flag
python trainer.py train --bundle synthetic.json --output new-test-job --epochs 20 --cancel-file cancel.flag --test-only
python trainer.py predict --model new-job/model.json --sample encoded-sample.json
python trainer.py evaluate --bundle frozen-test.json --models job-a/model.json job-b/model.json --output test_runs/compare-550e8400-e29b-41d4-a716-446655440000 --cancel-file cancel.flag
python -B -m unittest discover -s learning -v
```

stdout is flushed JSONL. Progress events contain event, percent and message.
Errors emit event=error/message and exit 1; cancellation emits event=cancelled
and exits 2. Probe exits 0 only after importing both dependencies. Prediction
outputs exactly a predictions map of corrected metric values (baseline + residual).
Prediction requires a sibling report.json with status=completed, matching
modelVersion and testOnly, productionEligible=false, and exactly the trained
target validation entries (matching model validation and finite error metrics).
An orphan model from an interrupted publication is refused by the CLI itself.
Keep model.json and report.json together when moving a completed artifact.
Input is SelfLearning::encode(sample), including complete input/baseline/measured;
no missing features or structure/version/bounds mismatch is silently defaulted.

Data-preparation partition contract: every training bundle row must explicitly
carry purpose=training; independent evaluation rows require
purpose=independent_test. Missing/unknown purpose, unassigned and excluded rows
are refused by the backend, not just disabled in the UI. Old learning datasets
without this metadata load in Qt as unassigned and require deliberate product
partitioning and a new export before training. Existing model prediction and
historical frozen training evidence are not rewritten or migrated. Qt freezes
new jobs from the persisted dataset under its writer lock and rejects stale
window snapshots before creating a job. The snapshot is the freeze point;
later product-purpose changes do not erase historical training provenance.

Independent evaluation never trains or selects weights. The output must be a
new directory whose name contains a UUID; evaluation.json is staged, fsynced,
and published by directory rename. Existing runs are refused. --cancel-file is
optional and cancellation returns 2 without publishing a report. --test-only
is required for marked bundles; the root testOnly flag also includes every
model's test flag. Test/synthetic evidence is never eligible.

The report follows ZTBLD-IndependentEvaluation v1 with status=completed,
absolute bundlePath and bundleSha256, and a models array. Each entry binds
absolute modelPath/reportPath and exact SHA256 digests, modelVersion, scope,
engineVersion, testOnly, eligible, failures, and targets. Failures are arrays
of readable strings (sample failures start with sample ID). All six target
entries contain labelledSamples, independentProducts, predictedSamples,
coverage (0..1), failures, and baseline/model statistics named MAE, RMSE,
maxAbsoluteError. Statistics are null when no observations exist. Baseline
statistics always cover every label, including failed predictions; model
statistics describe only successes and must be interpreted with coverage.
Zero baseline error is reported directly without a division/improvement ratio.

Every test product is checked against the union of ALL targets' trainGroups
and validationGroups using trimmed, casefolded product IDs. Missing, empty,
noncanonical or overlapping provenance blocks the whole model explicitly.
Missing targets, incompatible samples and out-of-bounds inputs are counted
against the full labelled denominator. Networks are loaded once per target
and cached within a model evaluation, with no per-sample subprocess. Inputs
are rehashed before publication to reject concurrent changes. eligible only
means complete independent evidence, never approval or activation. A reused
test bundle is no longer a blind final acceptance set; use history is managed
by the caller/registry. Python cannot attest external dataset use.

Run the independent CLI integration suite with the installed runtime, including
isolated mode (the fixture loader does not depend on current-directory imports):
`python -I -B learning/test_evaluation.py -v`.
Missing targets remain absent from predictions. No model is activated.

Training consumes only validated SelfLearning::trainingBundle v1 exports from
the trusted Qt/C++ producer. It checks metadata, inputSchema recursively,
all six baseline/label/residual fields and their finite consistency, canonical
productGroupId (trimmed casefolded productId), IDs and product/date uniqueness.
Python cannot rerun the C++ physics engine. datasetSha256 is recorded explicitly
as producer evidence, NOT claimed independently verified: Qt compact JSON number
encoding differs from Python canonical JSON. The exact bundle file SHA-256 is
also recorded. This trust boundary does not authenticate a hostile producer.

The fixed feature order is stored in model.json: capacity_kVA, hvRated_kV,
lvRated_kV, coreDiameter_mm, coreStraight_mm, stackFactor, steelThickness_mm,
hvBareWidth_mm, hvBareThick_mm, lvTurns, lvFoilThick_mm, lvFoilWidth_mm,
mainDuctWidth_mm, refFluxDens_T, hvCopperWire, lvCopperFoil. These represent
ratings, core/winding geometry and copper material flags. Prices, criteria,
identifiers, baseline outputs and measured labels are never input features.

Applicability is restricted to one fixed physical context. Every input field
outside those 16 features must match across ALL bundle rows, including steelGrade,
hvTurnsPerLayer, duct arrays, yokeSteps_mm and numeric craft/loss coefficients.
Conflicting keys cause an explicit error before training. This context is saved
as model.fixedContext; prediction requires exact matching values and array order.
Models created before this guard must be retrained. Changing an unrepresented
physical variable requires a separate homogeneous training bundle/model.

Only these nonphysical input keys are excluded from context comparison:
materialPrices, steelPricePerKg, useCustomSteelPrice, steelPriceGrade (cost and
price bindings), performanceCriteria, craftConstraints (acceptance/manufacturing
evidence), legacyUnitLossRecordText and legacyCraftCoefRecordText (annotations,
not the physical numeric coefficients). Sample metadata/source/IDs are outside
the input context. Such edits are permitted while still requiring schema-valid
input and valid sample provenance. Unknown input keys remain subject to schema
validation; any new schema-permitted physical keys are guarded by default.

Each eligible target uses a CPU 16→32→16→1 ReLU MLP with Adam/MSE. At least five
unique labelled products are required per target; others are reported skipped.
Sorted canonical groups are shuffled with seed 1729; floor(20%) (minimum one)
are held out for validation. All repeated products stay in one partition.
Feature and residual mean/std are fitted ONLY on training rows; zero variance
uses scale 1. We save the epoch with smallest validation MAE, compare with
formula baseline MAE and report model RMSE. Validation selects the epoch; it is
not an independent final test set. Reproducibility is for the same runtime;
PyTorch/platform upgrades may change floating point results.

Verified runtime: Python 3.12.5, torch 2.14.1+cpu, NumPy 2.5.2, device cpu.
Actual versions are recorded in both model.json and report.json under runtime
(pythonVersion, torchVersion, numpyVersion, device). requirements.txt ranges are
for portability; different installations are not claimed to reproduce results.

Weights are finite JSON arrays with strict tensor shape checks, never pickle.
Prediction requires every feature within that target's training min/max.
Job output may contain Qt-owned bundle.json/job.json; existing model/report
artifacts are never overwritten. A sibling staging directory is fully written
after training completes. A new directory is published by rename; in an existing
Qt job, model is renamed first and report last as the completion marker, with
rollback of files published by this attempt on error/cancellation. A hard process
kill between renames may leave model.json alone; consumers must require exit 0,
both files and Qt job status completed. Cancellation is checked each epoch
and before publishing. report.json root has status=completed, modelVersion,
modelPath, testOnly, productionEligible=false and validation per metric with
baselineMAE/modelMAE/RMSE. Exit 0 plus both complete files means success.

Synthetic fixtures exist only in test temporary directories. --test-only is
required for marked/synthetic-source bundles and propagates to both artifacts.
Unmarked data provenance remains the producer's responsibility. All models
remain experimental, productionEligible=false even with real data. Five groups
is only a running threshold, not evidence of engineering accuracy or deployment
eligibility. No optimizer or production-calculation integration exists. Qt's
separate review registry can approve a model only for an independent correction
display column after current enterprise criteria and independent evidence pass.
It binds artifacts and policy, records local reviewer/reason and audit events,
and revalidates activation/rollback; it never changes this backend's permanent
experimental/productionEligible=false metadata. Test models remain forbidden.

## Read-only candidate comparison (fifth batch)

```
python -I -B learning/trainer.py predict-batch --model MODEL/model.json --input snapshot.json --output JOBS/LOWERCASE-UUID [--cancel-file cancel.flag]
```

The output directory must be absent and its final name a canonical lowercase
UUID. Input `runId` must also be a canonical UUID. This command performs CPU
inference only; it never fits weights, changes model files, assigns product
training/test roles, ranks candidates, or approves a model. It requires the
completed, matching sibling `report.json` checked by `load_model`.

Input root fields are `format="ZTBLD-CandidateComparisonInput"`, `version=1`,
`usage="independent-comparison-only"`, `runId`, and `candidates` (1..10,000).
Each candidate contains `candidateId` (nonblank unique string), `schemeId`
(positive integer), `originalOrder` (integer exactly matching array order
1..N), `materialCost` (finite number >=0), `costBasis` (text preserved exactly),
and `sample`. The sample is the fully encoded original SelfLearning sample:
`id`, `productId`, `source="optimization-candidate"`, `schemeName`, ISO date
`testedOn`, supported `scope`/`engineVersion`, complete schema-valid `input`,
six original finite `baseline` metrics (losses/impedance >0, temperature rises
nonnegative), and six `measured` metrics all null. There is no training/reserved-test `purpose` requirement. Missing
or malformed snapshot fields reject the whole job without successful output.
Model physical context/structure and per-target bounds are applicability
failures, recorded separately for every affected target and original row.

On exit 0, `comparison.json` has exactly these root fields:

- `format="ZTBLD-CandidateComparison"`, `version=1`,
  `usage="independent-comparison-only"`, `status="completed"`, `runId`.
- `inputPath`, `inputSha256`, `modelPath`, `modelSha256`, `reportSha256`:
  resolved paths and lowercase SHA256 of the exact bytes. `reportSha256`
  binds the model's sibling `report.json`.
- `modelVersion`, `scope`, `engineVersion`, `testOnly` copied from the model;
  `productionEligible=false`. Model metadata remains permanently experimental,
  including when a separate display-only approval exists.
- `candidates`: every original row, in original array order, with exactly
  `candidateId`, `schemeId`, `originalOrder`, `materialCost`, `costBasis`,
  `baseline`, `predictions`, and `failures`.
- `counts`: `candidates`, `successfulTargets`, `failedTargets`. The two target
  counts sum to six times the candidate count.

`baseline` and `predictions` each have exactly these six keys:
`noLoadLoss_W`, `loadLoss_W`, `impedance_pct`, `oilTopRise_K`,
`hvWindingRise_K`, `lvWindingRise_K`. Every failed prediction is null and has
one readable reason under the same key in `failures`; successful keys are
absent from `failures`. Entirely failed candidates remain in the result.
Missing targets, incompatible physical context, bounds failures, and invalid
predictions never substitute zero or stale results. Corrected losses and
impedance must be strictly positive; corrected temperature rises may be zero.
This stricter corrected-value gate is specific to comparison. Shared sample
validation now also accepts physically valid zero original temperature rises;
a regression covers both batch and legacy single-sample prediction. Networks
are cached once per trained target across the batch. Results contain no measured labels or accuracy estimates.

Input and output JSON are each limited to 16 MiB, with no truncation. JSON
reads are bounded and duplicate keys/nonfinite values rejected. Cancellation
is checked during validation and each row/target, before staging, and just
before publication. All three evidence files are rehashed both before and
after staging. A sibling staging directory is flushed and atomically renamed
to the fresh UUID destination, with no existing output overwritten. On any
error/cancellation staging is removed and no completion is published. Exit
codes are 0=completed, 1=error, 2=cancelled; stdout uses JSON progress/error
records. Consumers must require successful process completion and independently
validate the frozen input/model/report bindings before showing corrections.

Temporary real CPU fixture tests (including isolated interpreter mode):

```
python -I -B learning/test_candidate_comparison.py -v
python -I -B -m unittest discover -s learning -p "test_*.py" -v
```
