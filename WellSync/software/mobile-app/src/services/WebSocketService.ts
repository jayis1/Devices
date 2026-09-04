export type LiveEvent = {
  type: string;
  state?: string;
  advisories?: string[];
};

export function connectLiveFeed(_url: string, onMessage: (event: LiveEvent) => void) {
  const mock: LiveEvent = {
    type: 'telemetry',
    state: 'watch',
    advisories: ['Collect a confirmation sample after storm runoff.'],
  };
  onMessage(mock);
  return { close: () => undefined };
}
