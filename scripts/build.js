import { spawn } from 'child_process';
import { promises as fs } from 'fs';
import os from 'os';

const dev = process.argv.includes('--dev');
const install = process.argv.includes('--install');
const signingEnvPath = './.keys/android-signing.env';

(async () => {
	try {
		if (dev) {
			await syncNetworkConfig();
			await updateAppId(true);
		} else {
			await ensureAndroidSigningEnv();
			await execCommand('vite build && npx cap sync');
		}
		const variant = dev ? 'debug' : 'release';
		const variantCap = dev ? 'Debug' : 'Release';
		await execCommand(`cd android && ${os.platform() === 'win32' ? 'gradlew' : './gradlew'} assemble${variantCap}`);
		if (install) {
			await installApk(variant);
		}
		if (dev) await updateAppId();
	} catch (e) {
		await updateAppId();
		await cleanupNetworkConfig();
		if (e instanceof Error) console.error(e.message);
		else console.error(e);
		process.exit(-1);
	}
})();

async function ensureAndroidSigningEnv() {
	const required = ['ANDROID_STORE_PASSWORD', 'ANDROID_KEY_PASSWORD'];
	if (required.every(key => process.env[key])) return;

	await loadSigningEnvFromFile();

	const missing = required.filter(key => !process.env[key]);
	if (missing.length === 0) return;

	throw new Error(
		`Missing Android signing values: ${missing.join(', ')}. ` +
		`Set env vars or create ${signingEnvPath} with KEY=VALUE pairs.`
	);
}

async function loadSigningEnvFromFile() {
	try {
		const content = await fs.readFile(signingEnvPath, 'utf8');
		for (const line of content.split(/\r?\n/)) {
			const trimmed = line.trim();
			if (!trimmed || trimmed.startsWith('#')) continue;
			const index = trimmed.indexOf('=');
			if (index <= 0) continue;
			const key = trimmed.slice(0, index).trim();
			const value = trimmed.slice(index + 1).trim();
			if (!process.env[key] && value) process.env[key] = value;
		}
	} catch {
		// local signing file is optional
	}
}

async function installApk(variant) {
	const apkPath = `android/app/build/outputs/apk/${variant}/app-${variant}.apk`;
	await fs.access(apkPath);
	await execCommand(`adb install -r "${apkPath}"`);
}

function execCommand(command) {
	return new Promise((resolve, reject) => {
		const child = spawn(command, { stdio: 'inherit', shell: true });
		child.on('error', reject);
		child.on('exit', code => {
			if (code === 0) {
				resolve();
			} else {
				reject(new Error(`Command exited with code ${code}`));
			}
		});
	});
}

const files = [
	'android/app/build.gradle',
	'android/app/src/main/res/values/strings.xml',
	'ios/App/App/Info.plist',
	'ios/App/App.xcodeproj/project.pbxproj',
];

async function updateAppId(dev = false) {
	for (const file of files) {
		let content = await fs.readFile(file, 'utf8');
		if (dev) {
			content = content.replaceAll(/(?<!namespace = ")dev\.tteles\.bikesensor(?!\.dev)/g, 'dev.tteles.bikesensor.dev');
			content = content.replaceAll(/Bike Sensor(?! Dev)/g, 'Bike Sensor Dev');
		} else {
			content = content.replaceAll(/(?<!namespace = ")dev\.tteles\.bikesensor\.dev/g, 'dev.tteles.bikesensor');
			content = content.replaceAll('Bike Sensor Dev', 'Bike Sensor');
		}
		await fs.writeFile(file, content, 'utf8');
	}
}

async function syncNetworkConfig() {
	const capacitorConfigRaw = await fs.readFile('./capacitor.config.json');
	await fs.copyFile('./capacitor.config.json', `./capacitor.config.json.timestamp-${Date.now()}`);
	const config = JSON.parse(String(capacitorConfigRaw));
	if (!config.server) config.server = {};
	config.server.url = `http://${getIp()}:${await getPort()}/`;
	config.server.cleartext = true;
	await fs.writeFile('./capacitor.config.json', JSON.stringify(config));
	await execCommand('npx cap sync');
	await cleanupNetworkConfig();
}

function getIp() {
	const ifaces = os.networkInterfaces();
	let ip = 'localhost';
	Object.keys(ifaces).forEach(ifname => {
		let alias = 0;
		const iface = ifaces[ifname];
		if (!iface || ifname.includes('vEthernet') || ifname.startsWith('veth') || ifname.startsWith('tailscale') || ifname.startsWith('br-') || ifname.startsWith('docker') || ifname == 'lo') return;
		iface.forEach(iface2 => {
			if ('IPv4' !== iface2.family || iface2.internal !== false) return;
			if (alias >= 1) ip = iface2.address;
			else ip = iface2.address;
			++alias;
		});
	});
	return ip;
}

async function getPort() {
	const file = await fs.readFile('./vite.config.ts');
	const match = String(file).match(/port:\s*(\d+)/);
	return match?.[1] ?? 5173;
}

async function cleanupNetworkConfig() {
	const files = await fs.readdir('./');
	for (const file of files) {
		if (file.match(/capacitor\.config\.json\.timestamp-\d+/g)) {
			await fs.copyFile(`./${file}`, './capacitor.config.json');
			await fs.unlink(file);
		}
	}
}