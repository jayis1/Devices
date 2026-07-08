import AsyncStorage from '@react-native-async-storage/async-storage';

const API_BASE = 'https://api.quakeguard.io';
const WS_URL = 'wss://api.quakeguard.io/ws';

export class QuakeGuardClient {
  private hubId: string | null = null;
  private ws: WebSocket | null = null;
  private listeners: ((data: any) => void)[] = [];

  async init() {
    this.hubId = await AsyncStorage.getItem('hubId');
  }

  async setHubId(hubId: string) {
    this.hubId = hubId;
    await AsyncStorage.setItem('hubId', hubId);
  }

  async getEvents(limit = 50) {
    const res = await fetch(`${API_BASE}/api/events?hub_id=${this.hubId}&limit=${limit}`);
    return res.json();
  }

  async getEvent(eventId: number) {
    const res = await fetch(`${API_BASE}/api/events/${eventId}`);
    return res.json();
  }

  async getNodes() {
    const res = await fetch(`${API_BASE}/api/nodes?hub_id=${this.hubId}`);
    return res.json();
  }

  async getStructuralReports(limit = 100) {
    const res = await fetch(`${API_BASE}/api/structural?hub_id=${this.hubId}&limit=${limit}`);
    return res.json();
  }

  async sendFamilyResponse(eventId: number, userId: string, status: 'safe' | 'need_help' | 'no_response') {
    const res = await fetch(`${API_BASE}/api/family/response`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        hub_id: this.hubId,
        event_id: eventId,
        user_id: userId,
        status,
      }),
    });
    return res.json();
  }

  async getStructuralReport() {
    const res = await fetch(`${API_BASE}/api/reports/structural?hub_id=${this.hubId}`);
    return res.json();
  }

  connectWebSocket(onMessage: (data: any) => void) {
    this.ws = new WebSocket(WS_URL);
    this.ws.onmessage = (event) => {
      const data = JSON.parse(event.data);
      onMessage(data);
    };
    this.ws.onclose = () => {
      // Reconnect after 5 seconds
      setTimeout(() => this.connectWebSocket(onMessage), 5000);
    };
  }

  disconnect() {
    this.ws?.close();
    this.ws = null;
  }
}

export const client = new QuakeGuardClient();