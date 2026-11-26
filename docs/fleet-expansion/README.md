# Fleet expansion: maxAI edge AI on every Bad Boy and Hustler mower

- `badboy-hustler-fleet-plan.html`: the review, with the model list, the signal matrix, an
  eight-team expert debate, hardware tiers, firmware layers and a roadmap. Open it in a browser.
- `fleet_profiles.json`: machine-readable source for the page. It lists models, engine families
  and, per canonical signal, whether J1939 provides it (`confirmed`, `inferred`, `verify`), whether
  it comes from analog inputs (`analog`, `switch`, `derived`), or whether it is missing (`none`).
  Phase 0 of the roadmap replaces every `verify` with logged facts.

Key findings:

1. Bad Boy signed to buy Excel Industries (Hustler) in September 2026. The last public report
   (11 Sept 2026) had closing pending regulatory approval. Use public data only until it closes.
2. Most units sold are carbureted: no ECU, no J1939. Every EFI engine in both lineups is air-cooled,
   so the coolant-based phase 1 model does not transfer to them.
3. Plan: four hardware tiers (maxAI 430, maxAI 280 or headless box, BLE health tag, electric bridge),
   one firmware platform with a machine profile layer, and edge-AI heads per signal family.
