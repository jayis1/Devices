export function connectDashboardSocket(url: string) {
  return {
    url,
    connect() {
      return `connecting to ${url}`;
    },
  };
}
