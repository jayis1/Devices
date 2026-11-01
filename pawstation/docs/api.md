# API and event contract

`POST /events` accepts JSON `{version,node_id,seq,kind,value,unit,utc_ms}`. Version must be 1; seq is non-negative; kind is one of `bag_level_pct`, `bin_fill_pct`, `lid_open`, `service_confirmed`; percent values are 0..100. Returns `202` when accepted, `409` duplicate, `422` malformed. Prototype endpoint is not authenticated; bind to localhost/private network only. Production must add user/device authentication and TLS.
