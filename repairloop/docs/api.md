# Local API

FastAPI routes: POST `/tickets` with `{category}` creates and persists an event-scoped anonymous ticket; GET `/tickets` lists FIFO queue; POST `/tickets/{id}/called` changes waiting to called once; DELETE `/tickets` clears event data. Invalid category receives 422; missing/already-called ticket receives 404. This prototype does not implement operator authentication, rate limiting, MQTT, remote access, or multi-event isolation. Restrict to a trusted LAN; add auth before shared or remote deployment.
