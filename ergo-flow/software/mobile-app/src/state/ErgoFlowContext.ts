/**
 * ErgoFlow — State Management (Zustand Store)
 * Central state for the mobile app
 *
 * Copyright (c) 2026 jayis1. MIT License.
 */

import { create } from 'zustand';
import { API_BASE_URL } from '../config';

export interface PostureReading {
  timestamp: string;
  score: number;
  posture_class: string;
  risk_level: number;
  duration_seconds: number;
}

export interface NodeStatus {
  id: string;
  address: string;
  online: boolean;
  battery_pct: number;
  firmware: string;
  uptime_hours: number;
}

export interface DeskStatus {
  height_mm: number;
  motor_state: string;
  current_ma: number;
}

export interface EnvironmentStatus {
  lux: number;
  temperature_c: number;
  humidity_pct: number;
}

export interface BreakInfo {
  breaks_today: number;
  breaks_completed: number;
  breaks_dismissed: number;
  compliance_pct: number;
  next_break_minutes: number;
}

export interface WeeklyReport {
  week_start: string;
  posture_grade: string;
  avg_posture_score: number;
  break_compliance_pct: number;
  focus_hours: number;
  rsi_risk_trend: number[];
  recommendations: string[];
}

interface ErgoFlowState {
  // Connection
  connected: boolean;
  hubAddress: string | null;

  // Real-time posture
  currentPosture: PostureReading | null;
  postureHistory: PostureReading[];

  // RSI risk
  rsiRiskScore: number;
  rsiRiskLevel: string;

  // Activity
  currentActivity: string;
  activityConfidence: number;

  // Focus
  focusLevel: string;
  focusScore: number;

  // Environment
  environment: EnvironmentStatus;

  // Desk
  deskStatus: DeskStatus;

  // Breaks
  breakInfo: BreakInfo;

  // Nodes
  nodes: NodeStatus[];

  // Weekly report
  weeklyReport: WeeklyReport | null;

  // Actions
  fetchStatus: () => Promise<void>;
  fetchPostureCurrent: () => Promise<void>;
  fetchPostureHistory: (minutes: number) => Promise<void>;
  fetchRsiRisk: () => Promise<void>;
  fetchActivity: () => Promise<void>;
  fetchFocus: () => Promise<void>;
  fetchEnvironment: () => Promise<void>;
  fetchDeskStatus: () => Promise<void>;
  fetchBreaks: () => Promise<void>;
  fetchWeeklyReport: () => Promise<void>;
  setDeskHeight: (height_mm: number) => Promise<void>;
  setDeskPreset: (preset: string) => Promise<void>;
  setLighting: (r: number, g: number, b: number, w: number, brightness: number, mode: string) => Promise<void>;
  setMonitorTilt: (degrees: number) => Promise<void>;
  dismissBreak: () => Promise<void>;
}

const useErgoFlowStore = create<ErgoFlowState>((set, get) => ({
  // Initial state
  connected: false,
  hubAddress: null,
  currentPosture: null,
  postureHistory: [],
  rsiRiskScore: 0,
  rsiRiskLevel: 'low',
  currentActivity: 'unknown',
  activityConfidence: 0,
  focusLevel: 'medium',
  focusScore: 50,
  environment: { lux: 0, temperature_c: 22, humidity_pct: 45 },
  deskStatus: { height_mm: 750, motor_state: 'idle', current_ma: 0 },
  breakInfo: { breaks_today: 0, breaks_completed: 0, breaks_dismissed: 0, compliance_pct: 0, next_break_minutes: 30 },
  nodes: [],
  weeklyReport: null,

  // API actions
  fetchStatus: async () => {
    try {
      const res = await fetch(`${API_BASE_URL}/api/v1/status`);
      const data = await res.json();
      set({ connected: true });
    } catch {
      set({ connected: false });
    }
  },

  fetchPostureCurrent: async () => {
    try {
      const res = await fetch(`${API_BASE_URL}/api/v1/posture/current`);
      const data = await res.json();
      set({ currentPosture: data });
    } catch (e) {
      console.error('Failed to fetch posture:', e);
    }
  },

  fetchPostureHistory: async (minutes: number = 60) => {
    try {
      const res = await fetch(`${API_BASE_URL}/api/v1/posture/history?minutes=${minutes}`);
      const data = await res.json();
      set({ postureHistory: data.readings });
    } catch (e) {
      console.error('Failed to fetch posture history:', e);
    }
  },

  fetchRsiRisk: async () => {
    try {
      const res = await fetch(`${API_BASE_URL}/api/v1/rsi-risk`);
      const data = await res.json();
      set({ rsiRiskScore: data.risk_score, rsiRiskLevel: data.risk_level });
    } catch (e) {
      console.error('Failed to fetch RSI risk:', e);
    }
  },

  fetchActivity: async () => {
    try {
      const res = await fetch(`${API_BASE_URL}/api/v1/activity/current`);
      const data = await res.json();
      set({ currentActivity: data.activity, activityConfidence: data.confidence });
    } catch (e) {
      console.error('Failed to fetch activity:', e);
    }
  },

  fetchFocus: async () => {
    try {
      const res = await fetch(`${API_BASE_URL}/api/v1/focus/current`);
      const data = await res.json();
      set({ focusLevel: data.focus_level, focusScore: data.score });
    } catch (e) {
      console.error('Failed to fetch focus:', e);
    }
  },

  fetchEnvironment: async () => {
    try {
      const res = await fetch(`${API_BASE_URL}/api/v1/environment`);
      const data = await res.json();
      set({ environment: data });
    } catch (e) {
      console.error('Failed to fetch environment:', e);
    }
  },

  fetchDeskStatus: async () => {
    try {
      const res = await fetch(`${API_BASE_URL}/api/v1/desk/status`);
      const data = await res.json();
      set({ deskStatus: data });
    } catch (e) {
      console.error('Failed to fetch desk status:', e);
    }
  },

  fetchBreaks: async () => {
    try {
      const res = await fetch(`${API_BASE_URL}/api/v1/breaks`);
      const data = await res.json();
      set({ breakInfo: data });
    } catch (e) {
      console.error('Failed to fetch breaks:', e);
    }
  },

  fetchWeeklyReport: async () => {
    try {
      const res = await fetch(`${API_BASE_URL}/api/v1/analytics/weekly`);
      const data = await res.json();
      set({ weeklyReport: data });
    } catch (e) {
      console.error('Failed to fetch weekly report:', e);
    }
  },

  setDeskHeight: async (height_mm: number) => {
    try {
      await fetch(`${API_BASE_URL}/api/v1/desk/height`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ cmd: 'height', target_mm: height_mm, speed_pct: 70 }),
      });
      get().fetchDeskStatus();
    } catch (e) {
      console.error('Failed to set desk height:', e);
    }
  },

  setDeskPreset: async (preset: string) => {
    try {
      await fetch(`${API_BASE_URL}/api/v1/desk/preset`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ cmd: 'preset', preset }),
      });
      get().fetchDeskStatus();
    } catch (e) {
      console.error('Failed to set desk preset:', e);
    }
  },

  setLighting: async (r: number, g: number, b: number, w: number, brightness: number, mode: string) => {
    try {
      await fetch(`${API_BASE_URL}/api/v1/lighting`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ r, g, b, w, brightness_pct: brightness, mode }),
      });
    } catch (e) {
      console.error('Failed to set lighting:', e);
    }
  },

  setMonitorTilt: async (degrees: number) => {
    try {
      await fetch(`${API_BASE_URL}/api/v1/monitor/tilt`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ tilt_degrees: degrees, speed_pct: 70 }),
      });
    } catch (e) {
      console.error('Failed to set monitor tilt:', e);
    }
  },

  dismissBreak: async () => {
    try {
      await fetch(`${API_BASE_URL}/api/v1/breaks/dismiss`, { method: 'POST' });
      get().fetchBreaks();
    } catch (e) {
      console.error('Failed to dismiss break:', e);
    }
  },
}));

export default useErgoFlowStore;