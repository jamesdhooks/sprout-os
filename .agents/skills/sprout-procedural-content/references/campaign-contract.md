# Campaign record

Retain at least:

~~~json
{
  "schemaVersion": 1,
  "gameId": "sprout.example",
  "contentId": "campaign-0001",
  "generator": "example-generator",
  "generatorVersion": 1,
  "seed": "123456",
  "difficulty": {
    "band": 1,
    "score": 0.1,
    "mechanics": [],
    "metrics": {}
  },
  "layout": {},
  "layoutHash": "..."
}
~~~

Automated checks should cover:

- deterministic regeneration;
- connectivity/solvability;
- solver/hint correctness;
- exact and near-duplicate detection;
- broad difficulty progression rather than strict per-level monotonicity;
- representative band boundaries;
- payload injection and source-hash guards;
- loader compatibility for every exported record.
