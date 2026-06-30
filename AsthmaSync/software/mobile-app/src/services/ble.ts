/**
 * AsthmaSync — BLE Service
 * Scans for and connects to AsthmaSync hub/nodes via Bluetooth Low Energy.
 * Used for direct phone-to-hub communication when WiFi is unavailable.
 */

import { Platform, NativeModules, PermissionsAndroid } from 'react-native';

// BLE UUIDs matching firmware
const ASTHMA_SVC_UUID     = '0000A501-0000-1000-8000-00805F9B34FB';
const TELEMETRY_CHAR_UUID = '00002A01-0000-1000-8000-00805F9B34FB';
const EVENT_CHAR_UUID      = '00002A03-0000-1000-8000-00805F9B34FB';
const COMMAND_CHAR_UUID   = '00002A02-0000-1000-8000-00805F9B34FB';

export class BleService {
  private static instance: BleService;
  private connectedDevice: string | null = null;
  private scanCallback: ((device: any) => void) | null = null;

  private constructor() {}

  static getInstance(): BleService {
    if (!BleService.instance) {
      BleService.instance = new BleService();
    }
    return BleService.instance;
  }

  async requestPermissions(): Promise<boolean> {
    if (Platform.OS !== 'android') return true;

    const granted = await PermissionsAndroid.requestMultiple([
      PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION,
      PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN,
      PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT,
    ]);

    return Object.values(granted).every(
      v => v === PermissionsAndroid.RESULTS.GRANTED
    );
  }

  async startScan(onDeviceFound: (device: any) => void): Promise<void> {
    const hasPermission = await this.requestPermissions();
    if (!hasPermission) {
      throw new Error('BLE permissions not granted');
    }

    this.scanCallback = onDeviceFound;

    // In production: use react-native-ble-plx
    // const { BleManager } = NativeModules;
    // BleManager.scan([ASTHMA_SVC_UUID], 10, true);
    console.log('BLE scan started for AsthmaSync devices');
  }

  async stopScan(): Promise<void> {
    // BleManager.stopScan();
    this.scanCallback = null;
    console.log('BLE scan stopped');
  }

  async connect(deviceId: string): Promise<void> {
    // await BleManager.connect(deviceId);
    // await BleManager.retrieveServices(deviceId);
    // await BleManager.startNotification(deviceId, ASTHMA_SVC_UUID, EVENT_CHAR_UUID);
    this.connectedDevice = deviceId;
    console.log('Connected to:', deviceId);
  }

  async disconnect(): Promise<void> {
    if (this.connectedDevice) {
      // await BleManager.cancelDeviceConnection(this.connectedDevice);
      this.connectedDevice = null;
    }
  }

  async sendCommand(command: string): Promise<void> {
    if (!this.connectedDevice) return;
    // const data = stringToBytes(command);
    // await BleManager.writeCharacteristicWithoutResponseForDevice(
    //   this.connectedDevice, ASTHMA_SVC_UUID, COMMAND_CHAR_UUID, base64.encode(data)
    // );
    console.log('BLE command sent:', command);
  }

  onTelemetry(callback: (data: any) => void): void {
    // In production: subscribe to notification stream
    // BleManager.onDidUpdateValueForCharacteristic(({ value, characteristic }) => {
    //   if (characteristic === TELEMETRY_CHAR_UUID) {
    //     const data = decodePayload(value);
    //     callback(data);
    //   }
    // });
  }
}