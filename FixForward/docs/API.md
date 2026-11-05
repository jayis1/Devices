# API v1
POST /v1/intake accepts JSON {id,category,weight_g,route}; route enum reuse|repair|battery_review|recycler; duplicate IDs return 409; bounds defined in Pydantic model. GET /v1/events returns local queue. Prototype has no authentication and must bind only to a trusted isolated LAN; never expose to the internet.
