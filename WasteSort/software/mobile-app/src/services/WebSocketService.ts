export type WasteSortEvent = {
  type: string;
  payload: Record<string, unknown>;
};

export class WebSocketService {
  private socket?: WebSocket;

  connect(url: string, onEvent: (event: WasteSortEvent) => void) {
    this.socket = new WebSocket(url);
    this.socket.onmessage = (message) => {
      try {
        const parsed = JSON.parse(message.data as string) as WasteSortEvent;
        onEvent(parsed);
      } catch {
        // ignore malformed events in scaffold
      }
    };
  }

  close() {
    this.socket?.close();
  }
}
