type MessageHandler = (data: any) => void;
type EventHandler = () => void;

export default class WebSocketService {
  private ws: WebSocket | null = null;
  private messageHandler: MessageHandler | null = null;
  private openHandler: EventHandler | null = null;
  private closeHandler: EventHandler | null = null;
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  private url: string = '';

  connect(url: string) {
    this.url = url;
    this.ws = new WebSocket(url);
    this.ws.onopen = () => this.openHandler?.();
    this.ws.onclose = () => {
      this.closeHandler?.();
      this.scheduleReconnect();
    };
    this.ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        this.messageHandler?.(data);
      } catch (e) { /* ignore parse errors */ }
    };
  }

  private scheduleReconnect() {
    if (this.reconnectTimer) return;
    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = null;
      if (this.url) this.connect(this.url);
    }, 5000);
  }

  onMessage(handler: MessageHandler) { this.messageHandler = handler; }
  onOpen(handler: EventHandler) { this.openHandler = handler; }
  onClose(handler: EventHandler) { this.closeHandler = handler; }

  send(data: object) {
    if (this.ws?.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify(data));
    }
  }

  disconnect() {
    if (this.reconnectTimer) { clearTimeout(this.reconnectTimer); }
    this.ws?.close();
  }
}