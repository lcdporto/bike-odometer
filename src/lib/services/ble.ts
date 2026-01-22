import { BleClient, type BleDevice, type ScanResult } from '@capacitor-community/bluetooth-le';

// UUID constants from ESP32 firmware
const SERVICE_UUID = '6a4e3200-9b5f-4c6a-9b7a-01c9b0a00001';
const CHARACTERISTIC_UUID = '6a4e3201-9b5f-4c6a-9b7a-01c9b0a00001';

export interface ScannedSensor {
	macAddress: string;
	name: string;
	strength: number;
	device: BleDevice;
}

/**
 * Initialize BLE client - must be called before using BLE functionality
 */
export async function initializeBLE(): Promise<void> {
	await BleClient.initialize();
}

/**
 * Request BLE permissions (required on Android)
 */
export async function requestBLEPermissions(): Promise<void> {
	try {
		await BleClient.initialize();
	} catch (error) {
		console.error('Failed to initialize BLE:', error);
		throw error;
	}
}

/**
 * Scan for BLE devices that start with "ESP32" and have the expected service UUID
 * @param scanDurationMs Duration to scan in milliseconds (default 5000ms)
 * @returns Array of discovered sensors
 */
export async function scanForESP32Sensors(scanDurationMs: number = 5000): Promise<ScannedSensor[]> {
	const discoveredSensors: ScannedSensor[] = [];

	try {
		console.log('[BLE] Starting scan for service UUID:', SERVICE_UUID);

		await BleClient.requestLEScan(
			{
				services: [SERVICE_UUID],
				allowDuplicates: false,
			},
			(result: ScanResult) => {
				const deviceId = result.device.deviceId;
				const deviceName = result.device.name || '(unnamed)';
				const rssi = result.rssi ?? -100;

				console.log(`[BLE] Device found: "${deviceName}" | ID: ${deviceId} | RSSI: ${rssi}dBm | Services: ${result.uuids?.join(', ') || 'none'}`);

				if (!discoveredSensors.some((s) => s.macAddress === deviceId)) {
					discoveredSensors.push({
						macAddress: deviceId,
						name: deviceName,
						strength: rssi,
						device: result.device
					});
				}
			}
		);

		// Wait for scan duration
		console.log(`[BLE] Scanning for ${scanDurationMs}ms...`);
		await new Promise((resolve) => setTimeout(resolve, scanDurationMs));

		// Stop scanning
		await BleClient.stopLEScan();
		console.log(`[BLE] Scan complete. Found ${discoveredSensors.length} ESP32 sensors`);
	} catch (error) {
		console.error('[BLE] Scan error:', error);
		throw error;
	}

	return discoveredSensors;
}

/**
 * Connect to a BLE device and read the sensor descriptor JSON from the characteristic
 * @param device The BLE device to connect to
 * @returns The sensor descriptor as a parsed JSON object
 */
export async function readSensorDescriptor<T = unknown>(device: BleDevice): Promise<T> {
	try {
		// Connect to device
		await BleClient.connect(device.deviceId, () => {
			console.log(`Device ${device.deviceId} disconnected`);
		});

		// Read the characteristic containing the JSON descriptor
		const dataView = await BleClient.read(device.deviceId, SERVICE_UUID, CHARACTERISTIC_UUID);

		// Convert DataView to string
		const decoder = new TextDecoder('utf-8');
		const jsonString = decoder.decode(dataView);
		console.log('Read sensor descriptor JSON:', jsonString);

		// Remove null bytes and any trailing garbage
		let cleanedString = jsonString.replace(/\0+$/, '').trim();

		// Try to find the last valid closing brace for the JSON object
		// This handles cases where there's garbage after the valid JSON
		try {
			// First, try to parse as-is
			const descriptor = JSON.parse(cleanedString) as T;
			return descriptor;
		} catch {
			// If that fails, try to find the last valid JSON by searching for the last }
			// and trimming everything after it
			let lastBrace = -1;
			let braceCount = 0;

			for (let i = 0; i < cleanedString.length; i++) {
				if (cleanedString[i] === '{' || cleanedString[i] === '[') {
					braceCount++;
				} else if (cleanedString[i] === '}' || cleanedString[i] === ']') {
					braceCount--;
					if (braceCount === 0) {
						lastBrace = i;
						break;
					}
				}
			}

			if (lastBrace !== -1) {
				cleanedString = cleanedString.substring(0, lastBrace + 1);
				console.log('Cleaned JSON string:', cleanedString);
			}
		}

		// Parse and return JSON
		const descriptor = JSON.parse(cleanedString) as T;

		// Disconnect after reading
		await BleClient.disconnect(device.deviceId);

		return descriptor;
	} catch (error) {
		console.error('Failed to read sensor descriptor:', error);
		// Ensure we disconnect even on error
		try {
			await BleClient.disconnect(device.deviceId);
		} catch {
			// Ignore disconnect errors
		}
		throw error;
	}
}

/**
 * Disconnect from all BLE devices
 */
export async function disconnectAll(): Promise<void> {
	// Note: BleClient doesn't have a disconnectAll method
	// You'll need to track connected devices and disconnect them individually
}
