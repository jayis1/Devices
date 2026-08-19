type MessageHandler = (data: any) => void;

export default class WebSocketService {
  private ws: WebSocket | null = null;
  private messageHandler: MessageHandler | null = null;
  private openHandler: (() => void) | null = null;
  private closeHandler: (() => void) | null = null;

  connect(url: string) {
    this.ws = new WebSocket(url);
    this.ws.onopen = () => this.openHandler?.();
    this.ws.onclose = () => this.closeHandler?.();
    this.ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        this.messageHandler?.(data);
      } catch (e) {
        console.warn('WebSocket parse error:', e);
      }
    };
  }

  onMessage(handler: MessageHandler) { this.messageHandler = handler; }
  onOpen(handler: () => void) { this.openHandler = handler; }
  onClose(handler: () => void) { this.closeHandler = handler; }

  send(data: any) {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify(data));
    }
  }

  disconnect() {
    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }
  }
}