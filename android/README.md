# sv

## Project branding

The app uses the shared [Bike Odometer logo](../docs/assets/logo.svg) for its
header, favicon, Android launcher icons, and splash screens. After editing the
logo, regenerate the bundled assets from this directory:

```sh
python3 scripts/generate-branding.py
```

This requires Python 3, Pillow, and `rsvg-convert` (librsvg). Generated assets are
checked in, so normal app builds do not need these tools. The generator updates
Android resources only; it does not regenerate the native iOS asset catalog.

---

Everything you need to build a Svelte project, powered by [`sv`](https://github.com/sveltejs/cli).

## Creating a project

If you're seeing this, you've probably already done this step. Congrats!

```sh
# create a new project in the current directory
npx sv create

# create a new project in my-app
npx sv create my-app
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

## Android release signing

Release APKs can be attached directly to a GitHub release. Set
`PUBLIC_BACKEND_API_URL` in `.env.local` before building; the deployed backend
used by the app is `https://bike-odometer.lcdporto.org`.

Keep a secure backup of `android/key.keystore` and `.keys/android-signing.env`.
Both are ignored by Git. Future releases must use the same signing key to update
an installed app without uninstalling it. Never attach these signing files to a
release. Increase `versionCode` and update `versionName` in
`android/app/build.gradle` for subsequent releases.

This project is configured like `gira-mais` CI for release signing:

- Gradle release signing reads:
	- `ANDROID_STORE_PASSWORD`
	- `ANDROID_KEY_PASSWORD`
- Keystore path is `android/key.keystore`
- Alias is `release`

### Local release build

1. Place your keystore at `android/key.keystore`.
2. Provide passwords using either env vars or a local file:

Option A (environment variables):

```sh
export ANDROID_STORE_PASSWORD='your_store_password'
export ANDROID_KEY_PASSWORD='your_key_password'
```

Option B (recommended for local dev): create `.keys/android-signing.env`:

```env
ANDROID_STORE_PASSWORD=your_store_password
ANDROID_KEY_PASSWORD=your_key_password
```

3. Build release:

```sh
npm run build-app
```

Signed APK is generated under:

- `android/app/build/outputs/apk/release/`

### Local debug install (bundled assets)

Build and install a debug APK that uses bundled web assets (no live dev server required):

```sh
npm run install-app-debug-local
```

Debug APK path:

- `android/app/build/outputs/apk/debug/app-debug.apk`

### GitHub Actions secrets

The workflow in `.github/workflows/build-mobile.yml` expects:

- `KEYSTORE_B64` (base64-encoded keystore file)
- `ANDROID_STORE_PASSWORD`
- `ANDROID_KEY_PASSWORD`

CI outputs are published from:

- `android/app/build/outputs/apk/release/`
- `android/app/build/outputs/bundle/release/`
