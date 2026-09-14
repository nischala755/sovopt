FROM node:22-alpine AS web
WORKDIR /src/web
COPY web/package*.json ./
RUN npm ci
COPY web/ ./
RUN npm run build

FROM python:3.12-slim AS runtime
RUN apt-get update && apt-get install -y --no-install-recommends build-essential cmake ninja-build && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY pyproject.toml ./
COPY python/ python/
COPY include/ include/
COPY src/ src/
COPY service/ service/
RUN python -m pip install --no-cache-dir . && apt-get purge -y build-essential cmake ninja-build && apt-get autoremove -y
COPY --from=web /src/web/dist /app/web/dist
ENV HOST=0.0.0.0 PORT=8000 SOVEREIGN_WEB_DIST=/app/web/dist SOVEREIGN_DATA_DIR=/var/lib/sovereign
EXPOSE 8000
CMD ["python", "-m", "service"]
