# sv

Everything you need to build a Svelte project, powered by [`sv`](https://github.com/sveltejs/cli).

## Creating a project

If you're seeing this, you've probably already done this step. Congrats!

```sh
# create a new project
npx sv create my-app
```

To recreate this project with the same configuration:

```sh
# recreate this project
bun x sv@0.12.4 create --template minimal --types ts --add tailwindcss="plugins:none" eslint --install bun .
```

## Developing

Once you've created a project and installed dependencies with `npm install` (or `pnpm install` or `yarn`), start a development server:

```sh
npm run dev

# or start the server and open the app in a new browser tab
npm run dev -- --open
```

## Building

To create a production version of your app:

```sh
npm run build
```

You can preview the production build with `npm run preview`.

> To deploy your app, you may need to install an [adapter](https://svelte.dev/docs/kit/adapters) for your target environment.

## Container Deploy

The current [compose.yaml](/home/gpereira/Documents/pcb-projects/bike-counter-app/apps/backend/compose.yaml) is for local builds because it uses `build:` and expects the full backend source tree next to it.

If you want the server to need only a compose file, publish the backend as an image first and deploy with [compose.deploy.yaml](/home/gpereira/Documents/pcb-projects/bike-counter-app/apps/backend/compose.deploy.yaml).

Build and publish the image from the project directory:

Before pushing to GHCR, authenticate with an account that is allowed to publish to the target namespace. For example, if you push to `ghcr.io/lcdporto/...`, the logged-in GitHub account or token must have permission to publish packages for `lcdporto`.

```sh
export GITHUB_USER=lcdporto
export CR_PAT=your_github_token_with_write_packages
echo "$CR_PAT" | docker login ghcr.io -u "$GITHUB_USER" --password-stdin
```

Exporting `GITHUB_USER` and `CR_PAT` by itself does nothing for Docker. You must run the `docker login ghcr.io ...` command, and it should print `Login Succeeded` before `docker buildx build --push ...` can upload anything.

If you previously logged in with the wrong account or a stale token, reset the GHCR login first:

```sh
docker logout ghcr.io
echo "$CR_PAT" | docker login ghcr.io -u "$GITHUB_USER" --password-stdin
```

Then build and push:

```sh
docker buildx build \
	--platform linux/amd64 \
	-t ghcr.io/lcdporto/bike-counter-backend:latest \
	--push \
	.
```

If `docker buildx build --platform ...` fails locally, your Docker CLI is missing the `buildx` plugin or is too old. In that case either install/enable Buildx, or if you are building on the same architecture as the server, use:

```sh
docker build -t ghcr.io/lcdporto/bike-counter-backend:latest .
docker push ghcr.io/lcdporto/bike-counter-backend:latest
```

Common GHCR push failures:

- `unauthorized: unauthenticated`: Docker is not logged into `ghcr.io`, or the token is invalid.
- `denied`: the token is valid, but it does not have permission to publish to that user or organization namespace.
- Organization namespace push fails: the org may require SSO authorization for the token.
- Exported token but still unauthenticated: you forgot to run `docker login ghcr.io`, or the login was done with a different GitHub username than the image namespace you are pushing to.

Example server-side deploy flow:

```sh
mkdir -p bike-counter-backend/data
cp compose.deploy.yaml bike-counter-backend/compose.yaml
cd bike-counter-backend
docker compose pull
docker compose up -d
```

If you want to pin a specific tag, set `BACKEND_IMAGE` in a `.env` file next to the compose file:

```env
BACKEND_IMAGE=ghcr.io/lcdporto/bike-counter-backend:2026-04-24
```

Then redeploy with:

```sh
docker compose pull
docker compose up -d
```
