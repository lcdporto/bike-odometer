import type { Sensor } from './sensor.svelte';
import { isPlaceholderDeviceName } from '$lib/utils';

export interface ConnectedDevice extends Sensor {
lastSeen: number;
}

class BLEDevicesStore {
connectedDevices = $state<ConnectedDevice[]>([]);
isScanning = $state(false);

// Derived values
count = $derived(this.connectedDevices.length);

// Actions
updateConnectedDevice(sensor: Sensor) {
const existingIndex = this.connectedDevices.findIndex((d) => d.id === sensor.id);

if (existingIndex !== -1) {
// Update existing device
		const existingDevice = this.connectedDevices[existingIndex];
		const nextName =
			isPlaceholderDeviceName(sensor.name) && !isPlaceholderDeviceName(existingDevice.name)
				? existingDevice.name
				: sensor.name;
this.connectedDevices[existingIndex] = {
...sensor,
			name: nextName,
lastSeen: Date.now()
};
} else {
// Add new device
this.connectedDevices = [...this.connectedDevices, {
...sensor,
lastSeen: Date.now()
}];
}
}

removeConnectedDevice(deviceId: string) {
this.connectedDevices = this.connectedDevices.filter((d) => d.id !== deviceId);
}

getConnectedDevice(deviceId: string): ConnectedDevice | undefined {
return this.connectedDevices.find((d) => d.id === deviceId);
}

clearConnectedDevices() {
this.connectedDevices = [];
}

setScanning(scanning: boolean) {
this.isScanning = scanning;
}

removeStaleDevices(maxAgeMs: number = 60000) {
const now = Date.now();
this.connectedDevices = this.connectedDevices.filter(
(device) => now - device.lastSeen < maxAgeMs
);
}
}

export const bleDevicesState = new BLEDevicesStore();
