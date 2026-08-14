/**
 * WebSocket Service
 * Real-time data streaming from PostureSync backend
 */

export class WebSocketService {
  private ws: WebSocket | null = null;
  private url: string;
  private onConnectCb: (() => void) | null = null;
  private onDisconnectCb: (() => void) | null = null;
  private onMessageCb: ((data: any) => void) | null = null;
  private reconnectTimer: any = null;

  constructor(url: string) {
    this.url = url;
  }

  connect() {
    this.ws = new WebSocket(this.url);

    this.ws.onopen = () => {
      console.log('WebSocket connected');
      this.onConnectCb?.();
    };

    this.ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        this.onMessageCb?.(data);
      } catch (e) {
        console.error('WebSocket parse error:', e);
      }
    };

    this.ws.onclose = () => {
      console.log('WebSocket disconnected');
      this.onDisconnectCb?.();
      // Reconnect after 5 seconds
      this.reconnectTimer = setTimeout(() => this.connect(), 5000);
    };

    this.ws.onerror = (error) => {
      console.error('WebSocket error:', error);
    };
  }

  disconnect() {
    if (this.reconnectTimer) clearTimeout(this.reconnectTimer);
    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }
  }

  onConnect(cb: () => void) { this.onConnectCb = cb; }
  onDisconnect(cb: () => void) { this.onDisconnectCb = cb; }
  onMessage(cb: (data: any) => void) { this.onMessageCb = cb; }

  send(data: any) {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify(data));
    }
  }
}