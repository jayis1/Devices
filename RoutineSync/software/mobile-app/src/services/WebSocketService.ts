export class WebSocketService {
  private socket?: WebSocket;

  connect(url: string, onMessage: (payload: unknown) => void): void {
    this.socket = new WebSocket(url);
    this.socket.onmessage = event => {
      try {
        onMessage(JSON.parse(event.data));
      } catch {
        onMessage(event.data);
      }
    };
  }

  close(): void {
    this.socket?.close();
  }
}
