# `typesafe/jev-1.13` — what it is, and where it earns its place here

Session 13, measured against `https://plusvibeapi.ru/v1/decisions` with the project's own key.

## It is not a chat model, and not a provider entry

Two facts that decide everything about how to use it:

* It is **absent from `/v1/models`** (73 models listed, none matching `jev`/`typesafe`), so it cannot
  be added as an OpenCode model the way `plusvibe/*` models are. It is not reachable as
  `model`/`small_model`/`agent` and cannot be a subagent brain.
* Its endpoint is **`POST /v1/decisions`**, a different contract entirely. Not
  `/v1/chat/completions`. The request is `{model, state, questions}` and the response is
  `{answers, usage}` — there is no message list, no free text, no tool calling, no code output.

So the honest answer to "можно ли её юзать в OpenCode как дополнительную модель" is **no** — and
"в тулзах" is the only place it fits, which is what you guessed.

## The contract, learned from the API's own errors

The server validates strictly and names the offending field, which is a good way to learn it:

```
{}                                              -> "Некорректное поле state: недопустимое значение."
{state:{}}                                      -> "Некорректное поле questions: ..."
{state:{}, questions:[]}                        -> "Некорректное поле questions: ..."   (must be an OBJECT)
{state:{}, questions:{q1:"..."}}                -> "Некорректное поле questions.q1: ..." (must be a typed object)
{... noul question WITH criteria}               -> "Вопроса типа noul принимает только instructions;
                                                    для именных вариантов используйте type \"choice\"."
```

Three question types, each carrying a different answer shape:

| type | fields | answer |
|---|---|---|
| `noul` | `instructions` only | a probability 0..1 (`noul`) |
| `choice` | `instructions`, `criteria: {name: desc}` | `choice` + full `probabilities` distribution + `confidence` |
| `score` | `instructions`, `criteria: [labels]` | `score` + distribution |

`state` is free text (a situation description), and answers are **distributions, not assertions** —
which is the interesting property for us.

## Live test on a real question we care about

I asked it about the rw4 blocker, in the exact shape of our documented claim (the two vtable slots are
map ops, the dispatcher has zero direct callers, Ghidra confirmed all three observations):

```json
{"status": 200, "answers": {
  "is_parser": {"type": "choice", "choice": "wrong_object",
                "probabilities": {"vtable_only": 0.36, "wrong_object": 0.61,
                                  "not_present": 0.0, "our_reading_wrong": 0.03},
                "confidence": 0.48},
  "seed_instead": {"type": "noul", "noul": 0.59}},
 "usage": {"input_tokens": 590, "output_tokens": 75}}
```

Read that carefully, because it is the whole story of this model in one response:

* It **disagrees** with the structural finding I had just concluded from the audit — it thinks we are
  looking at the wrong class (0.61) rather than hitting a vtable-reachability wall (0.36).
* Its `confidence` is **0.48**, i.e. barely better than a coin flip, and the distribution is nearly
  even. `our_reading_wrong` at 0.03 is the only firm signal: it is confident our *reading* of those
  slots is right.
* `seed_instead` = 0.59 — mildly in favour of the seeding we are doing anyway.
* Cost: 590 in / 75 out tokens, ~$0.04/1M in. Effectively free, and fast.

**Verdict on usefulness for this project: real but narrow, and it must be used as a prior, never as
evidence.** It is a *fast second opinion on a classification question*, priced so cheaply that asking
it costs nothing, and — critically — it returns a distribution instead of a confident sentence, which
is the honest shape for "I have two hypotheses and they are close". That is a much better fit for our
discipline than a chatty model guessing.

Where it would genuinely earn its place:

1. **Triage among candidate functions.** "Is this blob a serializer / a map op / a renderer /
   unrelated?" over a list of 20 addresses, before spending a real disassembly read on each. Cheap,
   parallel, and its distribution tells us when NOT to trust it (confidence < 0.6 → do the read).
2. **Format hypothesis ranking** when we have three competing explanations for a byte layout (the
   gmdl stride, the prop second section, the gmsh 12-residue mess) — as an ordering of hypotheses to
   test, never as the answer.
3. **A cross-check on our own conclusions**, exactly as above: it disagreed with me, and that is
   worth knowing *before* I build on the conclusion.

Where it is useless: it emits no code, no disassembly, no addresses, no text. It cannot read a binary,
cannot name a function, cannot write a decoder. Quality-of-code work — your own assessment — matches
what the API's shape implies: there is no code in the contract at all.

## Why it is NOT wired into the pipeline yet

Because the honest test above produced a **0.48-confidence disagreement with a finding that was
independently confirmed by Ghidra** (`direct_callers == 0` PASSED as a machine check). Ghidra's
confirmation is level-1 evidence from the bytes; jev is a prior. So it does not get a vote on
anything load-bearing. What it *does* get is a place in the backlog as a triage tool, and this file so
the next session does not rediscover the contract by trial and error.

If we do wire it up, the right shape is a `tools/jev.py` that (a) batches classification questions,
(b) **refuses to act on confidence < 0.6**, and (c) records the distribution in the doc alongside the
decision, so a weak prior never becomes a silent fact. The API key lives in
`C:\Users\DartPower\.config\opencode\opencode.json` under `provider.plusvibe.options.apiKey`; a
`tools/jev.py` should read it from there (or an env var) rather than hardcoding it into the repo.
