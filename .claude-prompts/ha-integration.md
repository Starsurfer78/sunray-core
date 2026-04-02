# Home Assistant Integration

Inherit global rules from `CLAUDE.md`.

Focus only on:
- MQTT Discovery entities
- stable operator semantics
- offline/reconnect behavior

Do:
- separate command entities from diagnostics
- prefer explicit state names

Do not:
- expose unsupported control paths
- imply safety guarantees that runtime does not prove
