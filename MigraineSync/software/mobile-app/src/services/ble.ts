/**
 * MigraineSync — BLE Service
 * ===========================
 * BLE relay for communicating with MigraineSync nodes when
 * the phone is away from the Hub (e.g., at work).
 *
 * Uses react-native-ble-plx for BLE communication.
 *
 * License: MIT
 */

import { BleManager } from 'react-native-ble-plx';

// MigraineSync BLE Service UUID
const SERVICE_UUID = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const TX_CHAR_UUID = '6e400002-b5a3-f393-e0a9-e50e24dcca9e';
const RX_CHAR_UUID = '6e400003-b5a3-f393-e0a9-e50e24dcca9e';

const bleManager = new BleManager();

export interface BLEDevice {
  id: string;
  name: string;
  rssi: number;
}

// ── Scan for MigraineSync nodes ──────────────────────────
export async function scanForNodes(timeoutMs: number = 5000): Promise<BLEDevice[]> {
  return new Promise((resolve, reject) => {
    const devices: BLEDevice[] = [];
    const found = new Set<string>();

    bleManager.startDeviceScan([SERVICE_UUID], null, (error, device) => {
      if (error) {
        reject(error);
        return;
      }

      if (device && !found.has(device.id)) {
        found.add(device.id);
        devices.push({
          id: device.id,
          name: device.name || 'MigraineSync Node',
          rssi: device.rssi || 0,
        });
      }
    });

    setTimeout(() => {
      bleManager.stopDeviceScan();
      resolve(devices);
    }, timeoutMs);
  });
}

// ── Connect to a node and subscribe to notifications ─────
export async function connectToNode(deviceId: string): Promise<void> {
  const device = await bleManager.connectToDevice(deviceId);
  await device.discoverAllServicesAndCharacteristics();

  await device.setupNotifications(TX_CHAR_UUID);

  device.monitorCharacteristicForService(
    SERVICE_UUID,
    TX_CHAR_UUID,
    (error, characteristic) => {
      if (error) {
        console.error('BLE monitor error:', error);
        return;
      }
      if (characteristic?.value) {
        // Decode base64 → parse frame → forward to cloud or local processing
        const data = Buffer.from(characteristic.value, 'base64');
        console.log('BLE RX:', data.length, 'bytes from', deviceId);
      }
    }
  );
}

// ── Send manual event to node via BLE ────────────────────
export async function sendManualEvent(deviceId: string, eventType: number): Promise<void> {
  // Build MANUAL_EVENT TLV and write to RX characteristic
  const payload = new Uint8Array([0x07, 0x06, eventType, 0, 0, 0, 0, 0]);
  const base64 = Buffer.from(payload).toString('base64');

  await bleManager.writeCharacteristicWithResponseForDevice(
    deviceId,
    SERVICE_UUID,
    RX_CHAR_UUID,
    base64
  );
}

export default bleManager;