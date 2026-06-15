/**
 * ErgoFlow — API Configuration
 */

export const API_BASE_URL = __DEV__
  ? 'http://10.0.2.2:8000'  // Android emulator → host
  : 'https://ergoflow.local:8000';  // Production

export const WS_URL = __DEV__
  ? 'ws://10.0.2.2:8000/ws/v1/realtime'
  : 'wss://ergoflow.local:8000/ws/v1/realtime';

export const BLE_SERVICE_UUID = '6E400001-B5A3-F393-E0A9-E50E24DCCA9E';
export const BLE_CHAR_TX = '6E400002-B5A3-F393-E0A9-E50E24DCCA9E';
export const BLE_CHAR_RX = '6E400003-B5A3-F393-E0A9-E50E24DCCA9E';

export const POLLING_INTERVAL_MS = 2000;  // 2 seconds
export const RECONNECT_DELAY_MS = 5000;