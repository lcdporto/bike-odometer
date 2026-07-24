import { BleClient, type BleDevice, type ScanResult } from '@capacitor-community/bluetooth-le';
import { isPlaceholderDeviceName } from '$lib/utils';

// UUID constants — byte-reversed from spec v1 (firmware transmits UUIDs little-endian)
const SERVICE_UUID =    '78563412-7856-3412-5678-123412345678';
const PULSE_UUID =      '78563412-7856-3412-5678-123412345679';
const TRIPS_UUID =      '78563412-7856-3412-5678-123412345688';
const BATTERY_UUID =    '78563412-7856-3412-5678-12341234568a';
const TIME_UUID =       '78563412-7856-3412-5678-12341234568b';
const ODOMETER_UUID =   '78563412-7856-3412-5678-12341234568c';
const WHEEL_SIZE_UUID = '78563412-7856-3412-5678-12341234568d';

// Trips download timeout (ms)
const TRIPS_TIMEOUT_MS = 30_000;


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
 * Scan for Bike_Odometer BLE devices advertising the primary service UUID
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

function decodeCharacteristicString(dataView: DataView): string {
	const decoder = new TextDecoder('utf-8');
	return decoder.decode(dataView).replace(/\0+$/g, '').trim();
}

function encodeCharacteristicString(value: string): DataView {
	const payload = new TextEncoder().encode(value);
	return new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
}

async function readConnectedStringCharacteristic(
	deviceId: string,
	characteristicUuid: string,
	label: string
): Promise<string> {
	const dataView = await BleClient.read(deviceId, SERVICE_UUID, characteristicUuid);
	const value = decodeCharacteristicString(dataView);
	console.log(`[BLE] Read ${label}:`, value);
	return value;
}

async function readStringCharacteristic(deviceId: string, characteristicUuid: string, label: string): Promise<string> {
	try {
		await BleClient.connect(deviceId, () => {
			console.log(`Device ${deviceId} disconnected`);
		});

		return await readConnectedStringCharacteristic(deviceId, characteristicUuid, label);
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
	const wheelSize = await readStringCharacteristic(deviceId, WHEEL_SIZE_UUID, 'wheel size descriptor');
	console.log(`[BLE] Wheel size descriptor for ${deviceId}:`, wheelSize);
	
	if (!wheelSize) {
		throw new Error(`Wheel size descriptor is empty for device ${deviceId}`);
	}
	return wheelSize;
}

export interface BatteryInfo {
	millivolts: number;
	percent: number;
}

function parseBatteryInfo(deviceId: string, value: string): BatteryInfo {
	const payload = JSON.parse(value) as { mv?: unknown; pct?: unknown };
	const millivolts = Number(payload.mv);
	const percent = Number(payload.pct);

	if (!Number.isFinite(millivolts) || !Number.isFinite(percent)) {
		throw new Error(`Invalid battery info from device ${deviceId}: ${value}`);
	}

	return {
		millivolts: Math.round(millivolts),
		percent: Math.max(0, Math.min(100, Math.round(percent)))
	};
}

export async function readBatteryInfo(deviceId: string): Promise<BatteryInfo> {
	const value = await readStringCharacteristic(deviceId, BATTERY_UUID, 'battery info');
	return parseBatteryInfo(deviceId, value);
}

export async function writeWheelSizeDescriptor(deviceId: string, wheelSizeInches: string): Promise<void> {
	await writeStringCharacteristic(
		deviceId,
		WHEEL_SIZE_UUID,
		wheelSizeInches.trim(),
		'wheel size descriptor'
	);
}

/**
 * Download all trips from a device using the chunked NOTIFY protocol.
 *
 * Flow:
 *   1. Connect
 *   2. Enable notifications on TRIPS_UUID (CCCD handled by BleClient)
 *   3. Write "start" to trigger the firmware to stream the JSON document
 *   4. Accumulate chunks until a complete JSON document is received
 *   5. Parse and return
 */
async function downloadTripsConnected<T>(deviceId: string): Promise<T> {
	try {
		let resolveTransfer!: (value: T) => void;
		const transferPromise = new Promise<T>((resolve) => {
			resolveTransfer = resolve;
		});

		const accumulatedBytes: number[] = [];

		await BleClient.startNotifications(deviceId, SERVICE_UUID, TRIPS_UUID, (dataView) => {
			for (let i = 0; i < dataView.byteLength; i++) {
				accumulatedBytes.push(dataView.getUint8(i));
			}

			const text = new TextDecoder('utf-8').decode(new Uint8Array(accumulatedBytes));

			// The spec says the document ends with ]}; use JSON.parse as the authority
			if (text.trimEnd().endsWith(']}')) {
				try {
					const parsed = JSON.parse(text) as T;
					console.log(`[BLE] Trips download complete; trips data:`, parsed);
					resolveTransfer(parsed);
				} catch {
					// Incomplete JSON that happens to end with ]}; keep accumulating
				}
			}
		});

		// Initiate transfer
		await BleClient.write(deviceId, SERVICE_UUID, TRIPS_UUID, encodeCharacteristicString('start'));
		console.log(`[BLE] Sent 'start' to ${deviceId}, awaiting trips chunks...`);

		const timeoutPromise = new Promise<never>((_, reject) =>
			setTimeout(() => reject(new Error(`Trips download timed out after ${TRIPS_TIMEOUT_MS}ms`)), TRIPS_TIMEOUT_MS)
		);

		const result = await Promise.race([transferPromise, timeoutPromise]);

		try {
			await BleClient.stopNotifications(deviceId, SERVICE_UUID, TRIPS_UUID);
		} catch {
			// Non-fatal if stop fails
		}

		return result;
	} catch (error) {
		console.error('[BLE] Failed to download trips:', error);
		throw error;
	}
}

export async function downloadTrips<T = unknown>(device: BleDevice): Promise<T> {
	const deviceId = device.deviceId;

	await BleClient.connect(deviceId, () => {
		console.log(`[BLE] Device ${deviceId} disconnected`);
	});

	try {
		return await downloadTripsConnected<T>(deviceId);
	} finally {
		try {
			await BleClient.disconnect(deviceId);
		} catch {
			// Ignore disconnect errors
		}
	}
}

export interface SensorSnapshot<T> {
	descriptor: T;
	wheelSize: string;
	battery: BatteryInfo;
}

/**
 * Synchronize all sensor data in one connection. The maximum-life firmware
 * stops advertising after a connection, so reconnecting for every
 * characteristic is both unreliable and unnecessarily expensive.
 */
export async function downloadSensorSnapshot<T>(device: BleDevice): Promise<SensorSnapshot<T>> {
	const deviceId = device.deviceId;

	await BleClient.connect(deviceId, () => {
		console.log(`[BLE] Device ${deviceId} disconnected`);
	});

	try {
		const unixTimestampSeconds = Math.floor(Date.now() / 1000);
		await BleClient.write(
			deviceId,
			SERVICE_UUID,
			TIME_UUID,
			encodeCharacteristicString(String(unixTimestampSeconds))
		);

		const batteryValue = await readConnectedStringCharacteristic(deviceId, BATTERY_UUID, 'battery info');
		const wheelSize = await readConnectedStringCharacteristic(deviceId, WHEEL_SIZE_UUID, 'wheel size');
		const descriptor = await downloadTripsConnected<T>(deviceId);

		return {
			descriptor,
			wheelSize,
			battery: parseBatteryInfo(deviceId, batteryValue)
		};
	} finally {
		try {
			await BleClient.disconnect(deviceId);
		} catch {
			// Ignore disconnect errors
		}
	}
}

/**
 * Disconnect from all BLE devices
 */
export async function disconnectAll(): Promise<void> {
	// Note: BleClient doesn't have a disconnectAll method
	// You'll need to track connected devices and disconnect them individually
}
