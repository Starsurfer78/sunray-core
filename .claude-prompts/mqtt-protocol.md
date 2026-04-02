# MQTT Protocol

Inherit global rules from `CLAUDE.md`.

Focus only on:
- topic hierarchy
- command validation
- disconnect/reconnect recovery
- heartbeat / retain / replay risk
- Home Assistant compatibility

Extra rules:
- unsafe command topics must not rely on broker correctness
- discuss retain policy explicitly where persistence matters
