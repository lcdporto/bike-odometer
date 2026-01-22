import type { Sensor } from './sensor.svelte';

interface ConnectedDevice extends Sensor {
	lastSeen: number;
}

interface BLEDevicesState {
	connectedDevices: ConnectedDevice[];
}

export const bleDevicesState: BLEDevicesState = $state({
	connectedDevices: []
});

/**
 * Add or update a device in the connected devices list
 */
export function updateConnectedDevice(sensor: Sensor) {
	const existingIndex = bleDevicesState.connectedDevices.findIndex((d) => d.id === sensor.id);
	
	if (existingIndex !== -1) {
		// Update existing device
		bleDevicesState.connectedDevices[existingIndex] = {
			...sensor,
			lastSeen: Date.now()
		};
	} else {
		// Add new device
		bleDevicesState.connectedDevices.push({
			...sensor,
			lastSeen: Date.now()
		});
	}
}

/**
 * Remove a device from connected devices list
 */
export function removeConnectedDevice(deviceId: string) {
	const index = bleDevicesState.connectedDevices.findIndex((d) => d.id === deviceId);
	if (index !== -1) {
		bleDevicesState.connectedDevices.splice(index, 1);
	}
}

/**
 * Get a device by ID
 */
export function getConnectedDevice(deviceId: string): ConnectedDevice | undefined {
	return bleDevicesState.connectedDevices.find((d) => d.id === deviceId);
}

/**
 * Clear all connected devices (useful for cleanup)
 */
export function clearConnectedDevices() {
	bleDevicesState.connectedDevices.length = 0;
}

/**
 * Remove stale devices that haven't been seen in a while
 */
export function removeStaleDevices(maxAgeMs: number = 60000) {
	const now = Date.now();
	bleDevicesState.connectedDevices = bleDevicesState.connectedDevices.filter(
		(device) => now - device.lastSeen < maxAgeMs
	);
}
