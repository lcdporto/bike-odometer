import { initializeBLE, scanForESP32Sensors, downloadTrips, readWheelSizeDescriptor } from './ble';
import { saveSensorDescriptor } from '$lib/persistence/sqlite';
import { syncSensorSnapshot } from '$lib/services/sync';
import type { Sensor } from '$lib/stores/sensor.svelte';
import type { BleDevice } from '@capacitor-community/bluetooth-le';
import { bleDevicesState } from '$lib/stores/ble-devices.svelte';
import { formatTime24h, getWheelCircumference, calculateDistance } from '$lib/utils';

const SCAN_INTERVAL = 10000; // 10 seconds between scans
const SCAN_DURATION = 5000; // 5 seconds per scan
const FIVE_MINUTES_MS = 5 * 60 * 1000;

// Convert RSSI (signal strength in dBm, typically -100 to -30) to percentage
function rssiToPercentage(rssi: number): number {
	const MIN_RSSI = -100;
	const MAX_RSSI = -30;
	const clampedRssi = Math.max(MIN_RSSI, Math.min(MAX_RSSI, rssi));
	return Math.round(((clampedRssi - MIN_RSSI) / (MAX_RSSI - MIN_RSSI)) * 100);
}

type SensorDescriptor = {
	trips: Array<{
		/**
		 * Trip ID
		 */
		id: number;
		/**
		 * Start date in seconds since 1970 epoch
		 */
		startDate: number;
		/**
		 * Number of rotations in 5 minute buckets
		 */
		buckets: number[];
	}>;
};

let isScanning = false;
let scanInterval: number | null = null;
const discoveredDevices = new Set<string>();

function normalizeWheelSizeValue(wheelSize: string): string {
	if (wheelSize.endsWith(".00")) {
		return wheelSize.slice(0, -3).trim();
	}
	return wheelSize.trim() || '26';
}

function toRotationBucket(trip: SensorDescriptor['trips'][0], idx: number) {
	const tripStartTimestamp = trip.startDate * 1000;
	const timestamp = tripStartTimestamp + idx * FIVE_MINUTES_MS;
	return {
		time: formatTime24h(new Date(timestamp)),
		rotations: trip.buckets[idx] || 0,
		timestamp
	};
}

function toTrip(trip: SensorDescriptor['trips'][0], wheelCircumference: number) {
	const buckets = trip.buckets.map((_, idx) => toRotationBucket(trip, idx));
	const totalRotations = buckets.reduce((sum, b) => sum + b.rotations, 0);
	const distance = calculateDistance(totalRotations, wheelCircumference);
	const duration = buckets.length * 5;
	const avgSpeed = duration > 0 ? (distance / duration) * 60 : 0;
	const startDate = new Date(trip.startDate * 1000);
	const endDate = new Date(startDate.getTime() + duration * 60 * 1000);

	return {
		id: `trip-${trip.id}`,
		date: startDate.toLocaleDateString('en-US', {
			weekday: 'short',
			month: 'short',
			day: 'numeric'
		}),
		startTime: formatTime24h(startDate),
		endTime: formatTime24h(endDate),
		distance,
		duration,
		avgSpeed,
		totalRotations,
		buckets
	};
}

/**
 * Process a discovered sensor by reading its data and saving to DB
 */
async function processSensor(deviceId: string, deviceName: string, strength: number, device: BleDevice) {
	try {
		console.log(`Processing sensor: ${deviceName} (${deviceId})`);
		
		// Download trips from device using chunked NOTIFY protocol
		const descriptor = await downloadTrips<SensorDescriptor>(device);
		console.log('Sensor descriptor received:', descriptor);
		const wheelSize = normalizeWheelSizeValue(await readWheelSizeDescriptor(device.deviceId));
		console.log('Wheel size descriptor received:', wheelSize);
		
		// Convert to app format
		const wheelCircumference = getWheelCircumference(wheelSize);
		
		const sensor: Sensor = {
			id: deviceId,
			name: deviceName,
			signalStrength: strength
		};
		
		const trips = descriptor.trips.map((trip) => toTrip(trip, wheelCircumference));
		
		// Save to database
		await saveSensorDescriptor(sensor, wheelSize, trips);
		console.log(`Sensor ${deviceName} data saved to DB`);

		// Sync latest snapshot to backend
		syncSensorSnapshot(sensor.id).catch((error) => {
			console.error(`Failed to sync sensor ${deviceName}:`, error);
		});
		
		// Mark as discovered so we don't process it again
		discoveredDevices.add(deviceId);
	} catch (error) {
		console.error(`Failed to process sensor ${deviceName}:`, error);
	}
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
			if (!discoveredDevices.has(sensor.macAddress)) {
				console.log(`New sensor discovered: ${sensor.name}`);
				// Process in background, don't wait
				processSensor(sensor.macAddress, sensor.name, sensor.strength, sensor.device).catch((err) => {
					console.error('Failed to process sensor:', err);
				});
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
