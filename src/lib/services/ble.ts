import { BleClient, type BleDevice, type ScanResult } from '@capacitor-community/bluetooth-le';
import { isPlaceholderDeviceName } from '$lib/utils';
import { toast } from "svelte-sonner";

// UUID constants from ESP32 firmware
const SERVICE_UUID = '78563412-7856-3412-5678-123412345678';
const CHARACTERISTIC_UUID = '78563412-7856-3412-5678-123412345688';
const DETECTION_TIMESTAMP_FIELD_UUID = '78563412-7856-3412-5678-12341234568b';
const WHEEL_SIZE_DESCRIPTOR_UUID = '1234568d-1234-5678-1234-567812345678';

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
				const rawName = result.device.name?.trim();
				const deviceName = isPlaceholderDeviceName(rawName) ? '(unnamed)' : rawName!;
				const rssi = result.rssi ?? -100;

				console.log(`[BLE] Device found: "${deviceName}" | ID: ${deviceId} | RSSI: ${rssi}dBm | Services: ${result.uuids?.join(', ') || 'none'} servicedata: ${JSON.stringify(result.serviceData)}`);

				if (!discoveredSensors.some((s) => s.macAddress === deviceId)) {
					discoveredSensors.push({
						macAddress: deviceId,
						name: deviceName,
						strength: rssi,
						device: result.device
					});

					writeDetectionTimestamp(deviceId);
				}
			}
		);

		try {
			console.log(`[BLE] Scanning for ${scanDurationMs}ms...`);
			await new Promise((resolve) => setTimeout(resolve, scanDurationMs));
		} finally {
			try {
				await BleClient.stopLEScan();
			} catch (stopError) {
				console.error('[BLE] Failed to stop LE scan:', stopError);
			}
		}

		console.log(`[BLE] Scan complete. Found ${discoveredSensors.length} ESP32 sensors`);
	} catch (error) {
		console.error('[BLE] Scan error:', error);
		throw error;
	}

	return discoveredSensors;
}

async function writeDetectionTimestamp(deviceId: string): Promise<void> {
	const unixTimestampSeconds = Math.floor(Date.now() / 1000);
	const payload = new TextEncoder().encode(String(unixTimestampSeconds));
	const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);

	try {
		await BleClient.connect(deviceId, () => {
			console.log(`Device ${deviceId} disconnected`);
		});

		const services = await BleClient.getServices(deviceId);
		const discoveredCharacteristics: string[] = [];

		for (const service of services) {
			for (const characteristic of service.characteristics ?? []) {
				discoveredCharacteristics.push(`${service.uuid} -> ${characteristic.uuid}`);
			}
		}

		if (discoveredCharacteristics.length > 0) {
			console.log(`[BLE] Discovered characteristics for ${deviceId}:`, discoveredCharacteristics);
		} else {
			console.log(`[BLE] No characteristics discovered for ${deviceId}`);
		}

		await BleClient.write(deviceId, SERVICE_UUID, DETECTION_TIMESTAMP_FIELD_UUID, view);
		toast(`Wrote detection timestamp to ${deviceId}`);

		console.log(`[BLE] Wrote detection timestamp ${unixTimestampSeconds} to ${deviceId}`);
	} catch (error) {
		console.error(`[BLE] Failed to write detection timestamp to ${deviceId}:`, error);
		toast(`Failed to write detection timestamp to ${deviceId}`);
	} finally {
		try {
			await BleClient.disconnect(deviceId);
		} catch {
			// Ignore disconnect errors
		}
	}
}

function decodeCharacteristicString(dataView: DataView): string {
	const decoder = new TextDecoder('utf-8');
	return decoder.decode(dataView).replace(/\0+$/g, '').trim();
}

function encodeCharacteristicString(value: string): DataView {
	const payload = new TextEncoder().encode(value);
	return new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
}

async function readStringCharacteristic(deviceId: string, characteristicUuid: string, label: string): Promise<string> {
	try {
		await BleClient.connect(deviceId, () => {
			console.log(`Device ${deviceId} disconnected`);
		});

		const dataView = await BleClient.read(deviceId, SERVICE_UUID, characteristicUuid);
		const value = decodeCharacteristicString(dataView);
		console.log(`[BLE] Read ${label}:`, value);
		return value;
	} catch (error) {
		console.error(`[BLE] Failed to read ${label}:`, error);
		throw error;
	} finally {
		try {
			await BleClient.disconnect(deviceId);
		} catch {
			// Ignore disconnect errors
		}
	}
}

async function writeStringCharacteristic(
	deviceId: string,
	characteristicUuid: string,
	value: string,
	label: string
): Promise<void> {
	try {
		await BleClient.connect(deviceId, () => {
			console.log(`Device ${deviceId} disconnected`);
		});

		await BleClient.write(
			deviceId,
			SERVICE_UUID,
			characteristicUuid,
			encodeCharacteristicString(value)
		);
		console.log(`[BLE] Wrote ${label}:`, value);
	} catch (error) {
		console.error(`[BLE] Failed to write ${label}:`, error);
		throw error;
	} finally {
		try {
			await BleClient.disconnect(deviceId);
		} catch {
			// Ignore disconnect errors
		}
	}
}

export async function readWheelSizeDescriptor(deviceId: string): Promise<string> {
	const wheelSize = await readStringCharacteristic(deviceId, WHEEL_SIZE_DESCRIPTOR_UUID, 'wheel size descriptor');
	if (!wheelSize) {
		throw new Error(`Wheel size descriptor is empty for device ${deviceId}`);
	}
	return wheelSize;
}

export async function writeWheelSizeDescriptor(deviceId: string, wheelSizeInches: string): Promise<void> {
	await writeStringCharacteristic(
		deviceId,
		WHEEL_SIZE_DESCRIPTOR_UUID,
		wheelSizeInches.trim(),
		'wheel size descriptor'
	);
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
		const jsonString = decodeCharacteristicString(dataView);
		console.log('Read sensor descriptor JSON:', jsonString);

		// Remove any trailing garbage
		let cleanedString = jsonString;

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
