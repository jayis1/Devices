export type OutageEvent = {
  type: string;
  payload: unknown;
};

export class WebSocketService {
  private ws?: WebSocket;
  connect(url: string, onEvent: (event: OutageEvent) => void) {
    this.ws = new WebSocket(url);
    this.ws.onmessage = (msg) => {
      try {
        onEvent(JSON.parse(msg.data));
      } catch {
        // ignore parse errors in stub
      }
    };
  }
  close() {
    this.ws?.close();
  }
}
