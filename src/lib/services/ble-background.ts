import { initializeBLE, scanForESP32Sensors, downloadTrips, readBatteryInfo, readWheelSizeDescriptor } from './ble';
import { tripFromSensorDescriptor, type SensorTripDescriptor } from '$lib/domain/trips';
import { saveSensorDescriptor } from '$lib/persistence/sqlite';
import { syncSensorSnapshot } from '$lib/services/sync';
import { applySensorWithTrips } from '$lib/state/app-state';
import type { Sensor } from '$lib/stores/sensor.svelte';
import { sensorState } from '$lib/stores/sensor.svelte';
import type { BleDevice } from '@capacitor-community/bluetooth-le';
import { bleDevicesState } from '$lib/stores/ble-devices.svelte';
import { getWheelCircumference } from '$lib/utils';

const SCAN_INTERVAL = 10000; // 10 seconds between scans
const SCAN_DURATION = 5000; // 5 seconds per scan

// Convert RSSI (signal strength in dBm, typically -100 to -30) to percentage
function rssiToPercentage(rssi: number): number {
	const MIN_RSSI = -100;
	const MAX_RSSI = -30;
	const clampedRssi = Math.max(MIN_RSSI, Math.min(MAX_RSSI, rssi));
	return Math.round(((clampedRssi - MIN_RSSI) / (MAX_RSSI - MIN_RSSI)) * 100);
}

type SensorDescriptor = {
	trips: SensorTripDescriptor[];
};

let isScanning = false;
let scanInterval: number | null = null;
const discoveredDevices = new Set<string>();
const processingDevices = new Map<string, Promise<boolean>>();

function normalizeWheelSizeValue(wheelSize: string): string {
	const trimmed = wheelSize.trim();
	const parsed = Number.parseFloat(trimmed);

	return Number.isFinite(parsed) ? parsed.toString() : '26';
}

/**
 * Process a discovered sensor by reading its data and saving to DB
 */
async function processSensor(deviceId: string, deviceName: string, strength: number, device: BleDevice) {
	console.log(`Processing sensor: ${deviceName} (${deviceId})`);

	const descriptor = await downloadTrips<SensorDescriptor>(device);
	console.log('Sensor descriptor received:', descriptor);
	const wheelSize = normalizeWheelSizeValue(await readWheelSizeDescriptor(device.deviceId));
	console.log('Wheel size descriptor received:', wheelSize);
	const battery = await readBatteryInfo(device.deviceId);
	console.log('Battery info received:', battery);

	const wheelCircumference = getWheelCircumference(wheelSize);
	const sensor: Sensor = {
		id: deviceId,
		name: deviceName,
		signalStrength: strength
	};
	const trips = descriptor.trips.map((trip) => tripFromSensorDescriptor(trip, wheelCircumference));

	await saveSensorDescriptor(sensor, wheelSize, trips);
	console.log(`Sensor ${deviceName} data saved to DB`);

	if (sensorState.connectedSensor?.id === sensor.id) {
		applySensorWithTrips({ sensor, wheelSize, trips }, sensorState.connectedSensor);
	}
	sensorState.setBatteryInfo(battery);

	syncSensorSnapshot(sensor.id).catch((error) => {
		console.error(`Failed to sync sensor ${deviceName}:`, error);
	});

	discoveredDevices.add(deviceId);
}

function queueSensorProcessing(deviceId: string, deviceName: string, strength: number, device: BleDevice) {
	const existing = processingDevices.get(deviceId);
	if (existing) return existing;

	const processing = processSensor(deviceId, deviceName, strength, device)
		.then(() => true)
		.catch((error) => {
			console.error(`Failed to process sensor ${deviceName}:`, error);
			return false;
		})
		.finally(() => {
			processingDevices.delete(deviceId);
		});

	processingDevices.set(deviceId, processing);
	return processing;
}

export function waitForSensorData(deviceId: string): Promise<boolean> {
	return processingDevices.get(deviceId) ?? Promise.resolve(discoveredDevices.has(deviceId));
}

/**
 * Perform a single scan iteration
 */
async function performScan() {
	if (isScanning) {
		console.log('Scan already in progress, skipping...');
		return;
	}
	
	try {
		isScanning = true;
		bleDevicesState.setScanning(true);
		console.log('Starting BLE scan...');
		
		const sensors = await scanForESP32Sensors(SCAN_DURATION);
		console.log(`Found ${sensors.length} ESP32 sensors`);
		
		// Process new sensors
		for (const sensor of sensors) {
			if (!discoveredDevices.has(sensor.macAddress) && !processingDevices.has(sensor.macAddress)) {
				console.log(`New sensor discovered: ${sensor.name}`);
				queueSensorProcessing(sensor.macAddress, sensor.name, sensor.strength, sensor.device);
			}
			
			// Update connected device in store
			bleDevicesState.updateConnectedDevice({
				id: sensor.macAddress,
				name: sensor.name,
				signalStrength: rssiToPercentage(sensor.strength)
			});
		}
		
		// Remove stale devices (not seen in last 30 seconds)
		bleDevicesState.removeStaleDevices(30000);
	} catch (error) {
		console.error('Scan failed:', error);
	} finally {
		isScanning = false;
		bleDevicesState.setScanning(false);
	}
}

/**
 * Start the background BLE scanning service
 */
export async function startBackgroundScanning() {
	if (scanInterval !== null) {
		console.log('Background scanning already running');
		return;
	}
	
	try {
		// Initialize BLE
		await initializeBLE();
		console.log('BLE initialized, starting background scanning...');
		
		// Perform initial scan
		await performScan();
		
		// Set up interval for continuous scanning
		scanInterval = window.setInterval(() => {
			performScan().catch((err) => {
				console.error('Background scan error:', err);
			});
		}, SCAN_INTERVAL);
		
		console.log('Background scanning started');
	} catch (error) {
		console.error('Failed to start background scanning:', error);
		throw error;
	}
}

/**
 * Stop the background BLE scanning service
 */
export function stopBackgroundScanning() {
	if (scanInterval !== null) {
		clearInterval(scanInterval);
		scanInterval = null;
		bleDevicesState.setScanning(false);
		console.log('Background scanning stopped');
	}
}

/**
 * Check if background scanning is active
 */
export function isBackgroundScanningActive(): boolean {
	return scanInterval !== null;
}

/**
 * Reset discovered devices (useful for testing or manual rescan)
 */
export function resetDiscoveredDevices() {
	discoveredDevices.clear();
	console.log('Discovered devices cleared');
}
