# Validation record

- Protocol host round-trip, corruption, truncation: PASS (`cc -std=c11 -Wall -Wextra -Werror ...`; executable returned 0).
- Python syntax: PASS (`python3 -m py_compile` on API, tests, and training script).
- API tests: PASS (2 passed; TestClient emitted a Starlette/httpx deprecation warning).
- ML synthetic demo: PASS (deterministic rule assertion; no efficacy claim).
- Mobile package JSON: PASS (`python3 -m json.tool`). TypeScript/TSX check: SKIPPED/FAILED because node --check does not handle `.tsx`; no app build performed.
- CSV shape, README table, links, secret/file-size scan: not yet verified.
- ESP-IDF/nRF firmware target builds, KiCad ERC, RF, calibration, safety, mobile platform build: not performed; toolchain/physical hardware validation absent.
- No performance claims are made. This remains a concept prototype.
