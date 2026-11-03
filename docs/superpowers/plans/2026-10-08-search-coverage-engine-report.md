# Task 1 engine/model implementation report

Worktree: `D:/CodexData/.codex/worktrees/composite-product-model/zt-transformer-ai-main`

Status: engine/model implementation ready for parent integration build and whole-branch review. Changes are uncommitted. This worker did not run builds, tests, mathematical experiments, applications, GUI automation, or subagents, and did not send messages to other chats.

## Files owned by this implementation

- `qt-app/src/engine/IOptimizer.h`: exact planned settings/defaults, saturation of the ninth numeric dimension and categorical multipliers, conditional validation, craft/retention/coverage summaries, batch signal/metatype, and shared numeric metadata helpers.
- `qt-app/src/engine/GridSearchSpace.h`: fixed eleven-coordinate identity with main duct appended at index 10, main-duct coarse/fine mapping, frozen craft settings in `inputFor`, boundary diagnostics, and interruptible legacy neighborhood generation.
- `qt-app/src/engine/SearchCenterPool.h` (new): coordinate-unique cost/order pool capped at 256 per grade, mandatory global-best center, rotating grade representatives and normalized farthest-region selection, per-phase center representation and eviction diagnostics, and distinct final pool statistics.
- `qt-app/src/engine/SearchCoveragePlanner.h` (new): strict physical hard-bound lattice conversion, lazy interleaved local/expansion generators, one-coarse-step outward faces, grade-specific explored frontiers, and actual processed physical-value envelopes.
- `qt-app/src/engine/GridOptimizer.cpp`: runtime unique processing budget, prefilters, bounded retained results, phase orchestration, interleaved expansion/local refinement, global budget progress throttling, bounded batch acknowledgement, total/grade statistics, and independent stop reasons.
- `qt-app/src/engine/ElectromagneticEngine.cpp`: craft validation/rejection at the full calculation entry and two print-note rows only. No physical formula changes.
- `qt-app/src/core/CalcInput.h`: frozen `CraftConstraints` snapshot member.
- `qt-app/src/core/CraftConstraints.h` (new): exact planned enabled/minimum/source members and validation/rejection/description methods.
- `qt-app/src/core/SchemeStore.h`: craft snapshot JSON save/load, legacy missing-object migration, explicit malformed-object rejection, and normalization of disabled nonfinite minimum values to zero on save.
- `qt-app/src/gui/widgets/ParamTableWidget.cpp`: subsequently authorized integration-review correction only, rendering the main-duct width with 17 significant digits to preserve load/collect precision at a strict craft minimum.
- This report is the sole documentation file written by this worker. Apart from that explicitly authorized widget precision correction, GUI, CMake, plan/spec and other concurrent parent changes were left to their owners.

## Public interface compatibility

All Task 1 planned settings, `CraftConstraints`, `craftRejected`, `retained`, `omitted`, `expansions`, `centerDetails`, `coverageDetails`, and `candidatesReady(const QVector<OptimizeCandidate>&)` use the specified names and types.

The agreed additive interface is `QString OptimizationRefinementSummary::phaseLabel`. Every enhanced local-fine/expansion phase populates it, including local fine re-entry after an expansion. Legacy refinement labels remain empty. Enhanced stage IDs are sequential generic phase counters; progress is percentage of the global processing budget. No further GUI interface variation is required.

Point indices remain 0..7 for existing numeric dimensions, 8 for round-wire specification, 9 for grade, with main duct appended at 10. Hard-text numeric index 8 maps to point index 10. Inactive/zero-radius variables stay fixed; old defaults and legacy traversal/tie ordering are preserved.

## Behavior and accounting

- Budget is checked before each new actual processing point. Only actual processed points enter the run-wide visited set. Phase-local reservations are separate, so planned but unprocessed points do not masquerade as evaluated points. Wire-form and craft prefilters both consume budget and do not count as engine failures.
- Legacy mode retains the startup conservative total upper-bound check and individual candidate delivery. Enhanced mode requires initial coarse count to fit the chosen budget, then schedules at most the remaining budget of unique points in each phase. Resource-limited scheduling explicitly reports unscheduled regions; no whole-space proof is implied.
- The retained-result max heap holds at most 5000 candidates by cost and first processing order. Total and per-grade accepted counts include all feasible candidates, including those omitted from display. The independent center pool sees every feasible point and holds at most 256 per grade; evictions are counted even when the just-arrived point is the one dropped.
- Global best is mandatory in center selection. Remaining seats rotate grade representatives before normalized regional diversity. Each phase reports center counts, per-grade actual processing/evaluations/feasible counts, before/after pool retention and evictions, and grades without centers. Final pool rows explicitly describe pool statistics, not another selection phase.
- Hard-bound validation runs only when both enhanced coverage and expansion are enabled. Center-limit and expansion-round checks are also gated by relevance. Raw inactive hard texts survive. A fixed legacy main duct is not newly forced positive; active main-duct search requires every initial/generated width to be finite and positive.
- Expansion alternates with local refinement. Low improvement ends only the current local sequence. Subsequent permitted expansion rounds continue, including representative rotation after a round with no new outward points. Faces must be strictly outside the touched frontier, move at most one original coarse step, and obey explicitly supplied hard limits. Round wire and steel-grade lists never expand.
- Enhanced Exhaustive plus enabled expansion reserves one fine coordinate level and performs one local refinement after each productive expansion. Initial fine phases still follow the selected original mode. CoarseFine/MultiRound use their selected local-round count on re-entry. This supports the specified expansion/refinement interleaving without changing the existing mode API.
- Generated unique plans and processed counts accumulate across coarse, all refinement/re-entry phases and the separate expansions list. Per-grade plans include scheduled points, even if manual stop interrupts evaluation. Stage elapsed time includes generation, processing, pause and batch acknowledgement; total elapsed is run wall time. Center selection and final reporting are also included in total wall time.
- Enhanced progress emits only when the global budget percentage changes. Worker-to-GUI batch delivery has at most one outstanding full snapshot: the forwarding lambda emits `candidatesReady` and then acknowledges a mutex/condition state. Stop wakes both pause and batch waits; no BlockingQueuedConnection is used. Shared acknowledgement state outlives a stopped/deleted worker, avoiding a late acknowledgement through a dangling QObject.
- `coverageDetails` lists each numeric dimension's initial range, user hard texts and actual processed physical-value envelope, including round-wire-driven width/thickness. Envelopes include prefilter rejections and are explicitly not claimed to be fully enumerated boxes. Budget exhaustion, manual stop, round limits, absent feasible seeds and lack of selected outward points have distinct explanatory text.
- All generated candidates receive `settings.craftConstraints` in `inputFor`. Old JSON without the object loads unchecked. Explicit malformed JSON becomes an enabled invalid sentinel and fails full engine recalculation. Enabled snapshots reject widths below the recorded enterprise minimum. Finite disabled minimum/source values survive; nonfinite disabled minimum saves as zero to avoid JSON-null corruption. Craft descriptions do not enter performance `skippedChecks`.

## Static self-review and handoff

Read Task 1 and Global Constraints before implementation, then the complete design spec. Reviewed all owned source diffs and the new modules; initially read GUI consumers to verify public field/signal/phase-label compatibility without editing them. The later integration review authorized only the focused main-duct precision correction in `ParamTableWidget.cpp`.

Reviewed the following invariants directly from the paths that update counters:

- `processed = evaluated + wireFormRejected + craftRejected <= combinationBudget`.
- `evaluated = accepted + invalid + constraintRejected` for each stage, each grade and the run.
- Run stage counters equal coarse plus all refinements (including expansion re-entry) plus separate expansions; the fine aggregate does not double-count expansions.
- `accepted = retained + omitted`; legacy retained equals accepted, enhanced retained is bounded by 5000.
- Phase `generated = planned + duplicateSkipped` for the actually enumerated/scheduled prefix; unenumerated regions are disclosed separately.
- Initial coarse order, lowest-cost legacy seeds, frozen prices/performance, round-wire and grade identities, and engine formulas retain their previous meaning.
- Hard lattice conversion restores adjacent physically valid endpoint cells and removes physically out-of-bound cells. Validated integer coordinate margins protect adjacent corrections, neighborhood offsets and outward coarse-step arithmetic.
- Manual stop during pause, generation, evaluation boundary or outstanding batch wakes/terminates the worker; batch-before-finished queued ordering delivers the last processed retained snapshot before the run summary.

`git diff --check` completed with exit 0 during static review. No build or runtime success is claimed here. Parent must perform a fresh integrated Release build and whole-branch static review on the final source state; prior parent builds predating worker changes do not validate this final implementation.

Limits remain explicit: capped center pools may lose higher-cost regions; selected representatives and touched faces are a heuristic search strategy; scheduled budget prefixes leave unplanned regions; neither stagnation, hard/frontier/round limits nor envelopes prove global optimality or complete manufacturing qualification.

## Final integration-review corrections

- Verified the main-duct load cell previously used default `QString::number` precision. It now uses `QString::number(input.mainDuctWidth_mm, 'g', 17)` so an unchanged searched width is rendered with enough significant digits for binary-double round-trip when collected through `QString::toDouble`. This avoids display rounding causing an unchanged saved candidate to fall below its frozen strict craft minimum.
- Inspected `saveToInput`'s `setDouble` and the design refresh paths. `setDouble` parses text directly and performs no 15-digit formatting. Design previews collect through this parser; their temporary validation table reloads through the same corrected load path and copies original cell texts verbatim. Normal/professional reloads also use this load path. No generic formatter or other searched width was changed.
- Enhanced phase `completed` now additionally requires `stage.planned > 0`, matching the legacy nonempty-stage condition. Empty or all-duplicate phases cannot be marked completed by the zero-equals-zero comparison.
- Craft's strict width comparison, physical formulas and all public interfaces remain unchanged. These corrections are uncommitted. Static diff/whitespace checks are the only verification performed by this worker; the parent performs the fresh integrated Release build and scoped re-review. No tests, application/GUI launches or subagents were used.
