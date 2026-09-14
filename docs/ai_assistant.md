# AI assistant boundary

AI support is optional and advisory. Configure it only through `MISTRAL_API_KEY`; the key is placed in the authorization header and never in prompts, stored metadata, telemetry, or API responses. Without a key or when Mistral fails, explanation endpoints return `available: false` and `explanation: null`. Solver and verification results remain unchanged.

`POST /ai/explain` sends only allowlisted aggregate fields: model dimensions/density, result status and scalar performance values, verification status, and aggregate benchmark timing/status data. Variable values, names, raw MPS text, certificates, arbitrary client fields, and filesystem paths are excluded. Remote text is an explanation, not proof; the engine's verification report remains authoritative.

The equivalent purpose-specific routes are `POST /ai/models/explain`,
`POST /ai/benchmarks/explain`, and `POST /ai/results/explain`.

`POST /ai/formulations/propose` accepts up to 4,000 characters and asks Mistral for JSON matching a strict schema. Extra fields and executable code are rejected. A valid response is stored temporarily with a random one-time `confirmation_token`; it does not create or solve a model.

The user must send that token to `POST /ai/formulations/confirm`. Only this explicit confirmation consumes the token and passes the validated structure to the native `Model` constructor. Unknown variables, invalid matrix entries, and invalid bounds are rejected by schema/native validation. Tokens are process-local and vanish on restart, which prevents an old unconfirmed proposal from being applied later.

For higher-assurance deployments, disable outbound network access when AI is unused, rotate the Mistral key through the deployment secret manager, authenticate these endpoints at the proxy, and log only request IDs and availability states.
