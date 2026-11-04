# Schematic status and connection planning

No native KiCad schematic is included; KiCad CLI was unavailable in the authoring environment. These are design notes, not electrical schematics or build instructions. See the system README for reference parts and explicit validation gaps.

Future capture should show per-node connector pin numbers, supply rails, decoupling, programming/debug access, pull-ups, battery protection, module antenna keep-out, and test points. Reconcile symbols, footprint variants, BOM references, and firmware pins before assembly. Obtain module manufacturer datasheets and review GPIO boot-strapping constraints before selecting pins. Do not connect the supply dock to any mains appliance.
