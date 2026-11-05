# React Native stub

Screen contract: local box search calls GET `/v1/search?q=<encoded text>` and renders `results[].{box_id,contents,room}`; scan flow upserts with PUT `/v1/boxes/{box_id}`. Pairing, offline queue UI, authentication, and native camera integration are not implemented. No package manifest is supplied, so this is not a buildable app yet.
