import type { Sensor } from './sensor.svelte';

export interface ConnectedDevice extends Sensor {
lastSeen: number;
}

class BLEDevicesStore {
connectedDevices = $state<ConnectedDevice[]>([]);

// Derived values
count = $derived(this.connectedDevices.length);

// Actions
updateConnectedDevice(sensor: Sensor) {
const existingIndex = this.connectedDevices.findIndex((d) => d.id === sensor.id);

if (existingIndex !== -1) {
// Update existing device
this.connectedDevices[existingIndex] = {
...sensor,
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

removeStaleDevices(maxAgeMs: number = 60000) {
const now = Date.now();
this.connectedDevices = this.connectedDevices.filter(
(device) => now - device.lastSeen < maxAgeMs
);
}
}

export const bleDevicesState = new BLEDevicesStore();
