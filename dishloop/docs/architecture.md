# DishLoop operation notes

Events are aggregate and approximate. Keep source records local by default. If optional MQTT is configured, publish only site-scoped aggregates using TLS and broker ACLs; never embed credentials in source. Retention, authenticated staff identity, node enrollment/key lifecycle, rate limits, persistence, backups and deletion/export must be implemented before deployment. Hub/network loss: physical workflow continues; nodes buffer bounded telemetry, and staff manually record wash batches. No node controls kitchen equipment.
