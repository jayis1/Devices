# Local API contract

POST /events accepts versioned event objects per README schema. Duplicate `(node,seq)` events return `duplicate:true`; malformed versions and values are rejected. GET /events returns at most 200 newest observations. API currently has no authentication and must bind only to localhost or a trusted isolated gym LAN. Never expose it to the public internet.
