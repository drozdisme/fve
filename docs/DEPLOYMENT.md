# Deployment

The platform is a single self-contained C++ service with no external runtime
dependencies. It serves the REST API and the static UI from one binary and
persists to a file-backed store by default; a PostgreSQL schema is provided for
the managed-database deployment.

## Local (from source)

```
make all
./bin/server ./data ./ui 8080
```

Then open `http://localhost:8080`. `./data` is created on first use and holds
artifacts, models, runs, provenance and the audit log.

Arguments: `server <data_dir> <ui_dir> <port>`.

## Docker

```
docker build -f deploy/Dockerfile -t fve:latest .
docker run -p 8080:8080 -v fve_data:/app/data fve:latest
```

The image is multi-stage: it builds and runs the full test suite in a `gcc:13`
stage, then copies only the binaries, the UI and the SQL into a slim runtime
image that runs as a non-root user.

## Docker Compose (API + PostgreSQL)

```
cd deploy
docker compose up --build
```

This starts the API on `:8080` and a `postgres:16` instance on `:5432` with the
migrations under `db/migrations/` applied on first boot via the Postgres
init-dir. The API reads `DATABASE_URL` from the environment.

## Kubernetes

```
kubectl create secret generic fve-db \
  --from-literal=url='postgres://fve:STRONG_PASSWORD@fve-db:5432/fve' \
  --from-literal=password='STRONG_PASSWORD'
kubectl apply -f deploy/k8s/
```

The manifests provision a `ConfigMap`, a 2-replica `Deployment` with readiness
and liveness probes on `/api/health`, a `Service`, an `Ingress`
(`fve.example.com`), a `PersistentVolumeClaim` for the data directory, and a
single-replica PostgreSQL `StatefulSet` with its own volume. Adjust the image
reference, host, and storage classes for the target cluster.

## CI/CD

`.github/workflows/ci.yml` builds with `gcc:13`, runs `make test`, runs a smoke
check of the `extract` and `verify` binaries, and on `main` builds and pushes the
Docker image to GHCR. Wire `GHCR_TOKEN` into repository secrets to enable the
push job.

## Configuration

Environment variables (see `deploy/production.env`):

```
FVE_DATA            data directory (default ./data)
FVE_UI              static UI directory (default ./ui)
FVE_PORT            listen port (default 8080)
DATABASE_URL        PostgreSQL connection string (managed-DB deployment)
FVE_PROBE_PER_DIM   wrapper-synthesis probes per input dimension
FVE_WRAPPER_SAFETY  Lipschitz safety multiplier for wrapper enclosures
FVE_MAX_UPLOAD_MB   maximum upload size
```

## Database migrations

Apply the numbered migrations in order against the target database:

```
psql "$DATABASE_URL" -f db/migrations/0001_init.sql
psql "$DATABASE_URL" -f db/migrations/0002_provenance_audit.sql
```

`db/schema.sql` is the consolidated current schema for reference.

## Health and smoke

```
curl -s localhost:8080/api/health
curl -s -X POST localhost:8080/api/upload -H 'X-Filename: beam.xlsx' \
     --data-binary @tests/fixtures/beam.xlsx
```
