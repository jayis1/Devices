import { BleManager } from 'react-native-ble-plx';

// CompostSync BLE service UUID
const SERVICE_UUID = '0000c580-1212-efde-1523-785feabcd123';
const CHAR_READ_UUID = '0000c581-1212-efde-1523-785feabcd123';
const CHAR_WRITE_UUID = '0000c582-1212-efde-1523-785feabcd123';
const CHAR_NOTIFY_UUID = '0000c583-1212-efde-1523-785feabcd123';

class BLEClient {
  manager: BleManager;
  connectedDevice: any = null;

  constructor() {
    this.manager = new BleManager();
  }

  async scanAndConnect(): Promise<boolean> {
    return new Promise((resolve) => {
      this.manager.startDeviceScan([SERVICE_UUID], null, (error, device) => {
        if (error) {
          console.error('BLE scan error:', error);
          resolve(false);
          return;
        }
        if (device?.name?.includes('CompostSync')) {
          this.manager.stopDeviceScan();
          this.connect(device).then(resolve);
        }
      });

      // Timeout after 10 seconds
      setTimeout(() => {
        this.manager.stopDeviceScan();
        resolve(false);
      }, 10000);
    });
  }

  async connect(device: any): Promise<boolean> {
    try {
      this.connectedDevice = await device.connect();
      await this.connectedDevice.discoverAllServicesAndCharacteristics();
      console.log('Connected to CompostSync Hub:', device.id);
      return true;
    } catch (e) {
      console.error('BLE connect failed:', e);
      return false;
    }
  }

  async readSnapshot(): Promise<any> {
    if (!this.connectedDevice) return null;
    try {
      const char = await this.connectedDevice.readCharacteristicForService(
        SERVICE_UUID, CHAR_READ_UUID
      );
      return JSON.parse(char?.value || '{}');
    } catch (e) {
      console.error('BLE read failed:', e);
      return null;
    }
  }

  async sendCommand(cmd: string): Promise<void> {
    if (!this.connectedDevice) return;
    try {
      await this.connectedDevice.writeCharacteristicWithResponseForService(
        SERVICE_UUID, CHAR_WRITE_UUID, btoa(cmd)
      );
    } catch (e) {
      console.error('BLE write failed:', e);
    }
  }

  async enableNotifications(callback: (data: any) => void): Promise<void> {
    if (!this.connectedDevice) return;
    try {
      await this.connectedDevice.monitorCharacteristicForService(
        SERVICE_UUID, CHAR_NOTIFY_UUID, (error, char) => {
          if (error) return;
          if (char?.value) {
            try {
              callback(JSON.parse(atob(char.value)));
            } catch {}
          }
        }
      );
    } catch (e) {
      console.error('BLE notify setup failed:', e);
    }
  }

  disconnect(): void {
    if (this.connectedDevice) {
      this.connectedDevice.cancelConnection();
      this.connectedDevice = null;
    }
    this.manager.stopDeviceScan();
  }
}

export const bleClient = new BLEClient();