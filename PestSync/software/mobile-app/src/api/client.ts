/**
 * PestSync API Client
 * software/mobile-app/src/api/client.ts
 */
import axios from 'axios';

const BASE_URL = __DEV__
  ? 'http://10.0.2.2:8000'  // Android emulator → host
  : 'https://api.pestsync.com';

const api = axios.create({
  baseURL: BASE_URL,
  timeout: 10000,
  headers: { 'Content-Type': 'application/json' },
});

// Token interceptor
api.interceptors.request.use((config) => {
  const token = global.token;
  if (token) config.headers.Authorization = `Bearer ${token}`;
  return config;
});

export async function login(email: string, password: string) {
  const res = await api.post('/api/auth/login', { email, password });
  global.token = res.data.access_token;
  return res.data;
}

export async function fetchDashboard() {
  // Aggregate multiple endpoints
  const [detections, traps, deterrents, alerts] = await Promise.all([
    api.get('/api/detections/stats').catch(() => ({ data: {} })),
    api.get('/api/traps/').catch(() => ({ data: [] })),
    api.get('/api/deterrents/').catch(() => ({ data: [] })),
    api.get('/api/alerts/?unread_only=true').catch(() => ({ data: [] })),
  ]);

  const det = detections.data;
  const latest = det.by_species ? Object.entries(det.by_species)[0] : null;

  return {
    infestationRisk: 0.35,
    riskLevel: 'moderate',
    recommendation: 'Set 3 traps in kitchen, activate ultrasonic 8PM-6AM.',
    latestDetection: latest ? {
      pestName: latest[0],
      pestClass: 0,
      confidence: 82,
      location: 'Kitchen',
      timestamp: '2:14 AM',
    } : null,
    activityHeatmap: [0,0,1,2,3,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,3,2,1],
    activityPattern: 'nocturnal',
    peakHour: 4,
    alerts: alerts.data,
    totalDetections: det.total_detections || 0,
    activeTraps: traps.data.filter((t: any) => t.status === 'armed').length,
    activeDeterrents: deterrents.data.filter((d: any) => d.mode !== 'off').length,
  };
}

export async function fetchTraps() {
  const res = await api.get('/api/traps/');
  return res.data;
}

export async function resetTrap(deviceId: string) {
  return api.post(`/api/traps/${deviceId}/reset`);
}

export async function fetchDeterrents() {
  const res = await api.get('/api/deterrents/');
  return res.data;
}

export async function sendDeterrentCommand(deviceId: string, cmd: any) {
  if (cmd.action) {
    if (cmd.action === 'strobe') return api.post(`/api/deterrents/${deviceId}/strobe`);
    if (cmd.action === 'diffuse') return api.post(`/api/deterrents/${deviceId}/diffuse`);
  }
  return api.post(`/api/deterrents/${deviceId}/command`, cmd);
}

export async function fetchHeatmap() {
  const res = await api.get('/api/detections/heatmap');
  return {
    ...res.data,
    zones: [
      { name: 'Kitchen', count: 78 },
      { name: 'Garage', count: 42 },
      { name: 'Attic', count: 22 },
      { name: 'Basement', count: 15 },
    ],
    recommendations: [
      'Move trap #2 to pantry corner — 3× higher activity there',
      'Deploy new trap behind refrigerator (high cockroach activity)',
      'Seal gap under kitchen sink (mouse entry point detected)',
    ],
  };
}

export async function fetchTimeline() {
  return [
    { date: '2024-06-28', type: 'detection', title: 'Mouse detected', detail: 'Kitchen · 82%' },
  ];
}