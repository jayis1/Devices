// UI contract stub only. Expected screens: site overview, stale-node list,
// prioritized tree tasks, visit confirmation. No real app runtime configured.
export type Observation = { v: 1; node: string; seq: number; ts: number; kind: 'soil'|'weather'; soil_pct?: number; temp_c?: number; rh_pct?: number; rain_mm?: number };
export async function fetchObservations(baseUrl: string, site: string): Promise<Observation[]> {
  const response = await fetch(`${baseUrl}/sites/${encodeURIComponent(site)}/observations`);
  if (!response.ok) throw new Error(`API request failed: ${response.status}`);
  return response.json();
}
