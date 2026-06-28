/**
 * PestSync BLE Client
 * software/mobile-app/src/ble/bleClient.ts
 *
 * Connects directly to PestSync Hub via BLE 5.0 for local access.
 * Uses react-native-ble-plx.
 */
import { BleManager } from 'react-native-ble-plx';

const PESTSYNC_SERVICE_UUID = '000050e0-0000-1000-8000-00805f9b34fb';
const READ_CHAR_UUID = '000050e1-0000-1000-8000-00805f9b34fb';
const WRITE_CHAR_UUID = '000050e2-0000-1000-8000-00805f9b34fb';
const NOTIFY_CHAR_UUID = '000050e3-0000-1000-8000-00805f9b34fb';

const bleManager = new BleManager();

let connectedDevice: any = null;

export async function scanForHubs(timeoutMs: number = 10000): Promise<any[]> {
  return new Promise((resolve) => {
    const devices: any[] = [];
    bleManager.startDeviceScan([PESTSYNC_SERVICE_UUID], null, (error, device) => {
      if (error) {
        console.warn('BLE scan error:', error);
        resolve(devices);
        return;
      }
      if (device && !devices.find(d => d.id === device.id)) {
        devices.push(device);
      }
    });
    setTimeout(() => {
      bleManager.stopDeviceScan();
      resolve(devices);
    }, timeoutMs);
  });
}

export async function connectBLE(deviceId: string): Promise<void> {
  const device = await bleManager.connectToDevice(deviceId);
  await device.discoverAllServicesAndCharacteristics();
  connectedDevice = device;
  console.log('BLE connected to PestSync Hub:', deviceId);
}

export async function readSnapshot(): Promise<any> {
  if (!connectedDevice) throw new Error('Not connected');
  const char = await connectedDevice.readCharacteristicForService(
    PESTSYNC_SERVICE_UUID, READ_CHAR_UUID
  );
  if (char?.value) {
    const json = Buffer.from(char.value, 'base64').toString('utf-8');
    return JSON.parse(json);
  }
  return null;
}

export async function sendCommand(command: any): Promise<void> {
  if (!connectedDevice) throw new Error('Not connected');
  const json = JSON.stringify(command);
  const base64 = Buffer.from(json, 'utf-8').toString('base64');
  await connectedDevice.writeCharacteristicWithResponseForService(
    PESTSYNC_SERVICE_UUID, WRITE_CHAR_UUID, base64
  );
}

export async function subscribeToCharacteristic(
  type: string,
  callback: (data: any) => void
): Promise<void> {
  if (!connectedDevice) throw new Error('Not connected');
  await connectedDevice.monitorCharacteristicForService(
    PESTSYNC_SERVICE_UUID, NOTIFY_CHAR_UUID,
    (error, characteristic) => {
      if (error) {
        console.warn('BLE monitor error:', error);
        return;
      }
      if (characteristic?.value) {
        const json = Buffer.from(characteristic.value, 'base64').toString('utf-8');
        try {
          callback(JSON.parse(json));
        } catch {
          callback({ raw: json });
        }
      }
    }
  );
}

export async function disconnect(): Promise<void> {
  if (connectedDevice) {
    await bleManager.cancelDeviceConnection(connectedDevice.id);
    connectedDevice = null;
  }
}

export function isConnected(): boolean {
  return connectedDevice !== null;
}