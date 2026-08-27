export type LiveEvent = {
  type: string;
  payload: Record<string, unknown>;
};

export class WebSocketService {
  private socket?: WebSocket;

  connect(url: string, onMessage: (event: LiveEvent) => void) {
    this.socket = new WebSocket(url);
    this.socket.onmessage = (message) => {
      onMessage(JSON.parse(message.data) as LiveEvent);
    };
  }

  disconnect() {
    this.socket?.close();
  }
}
