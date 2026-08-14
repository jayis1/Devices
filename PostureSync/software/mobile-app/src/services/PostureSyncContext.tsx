/**
 * PostureSync Context Provider
 * Global state management + API client
 */

import React, { createContext, useState, useEffect, ReactNode } from 'react';

interface PostureData {
  posture_class: number;
  spine_angles: { pitch: number; roll: number; yaw: number };
  hr: number;
  spo2: number;
}

interface EMGData {
  emg_rms: number[];
  asymmetry_pct: number;
}

interface PostureSyncContextType {
  postureData: PostureData | null;
  postureScore: number;
  emgData: EMGData | null;
  api: APIClient;
}

interface APIClient {
  getRiskForecast: () => Promise<any>;
  getSpinalAge: () => Promise<any>;
  getScoliosisScreen: () => Promise<any>;
  getCoaching: () => Promise<any>;
  triggerCorrection: (pattern: number) => Promise<any>;
}

const PostureSyncContext = createContext<PostureSyncContextType>({
  postureData: null,
  postureScore: 100,
  emgData: null,
  api: {} as APIClient,
});

export function PostureSyncProvider({ children, apiBase }: { children: ReactNode; apiBase: string }) {
  const [postureData, setPostureData] = useState<PostureData | null>(null);
  const [postureScore, setPostureScore] = useState(100);
  const [emgData, setEMGData] = useState<EMGData | null>(null);

  const api: APIClient = {
    getRiskForecast: async () => {
      const res = await fetch(`${apiBase}/api/v1/risk/forecast`);
      return res.json();
    },
    getSpinalAge: async () => {
      const res = await fetch(`${apiBase}/api/v1/risk/spinal-age`);
      return res.json();
    },
    getScoliosisScreen: async () => {
      const res = await fetch(`${apiBase}/api/v1/scoliosis/screen`);
      return res.json();
    },
    getCoaching: async () => {
      const res = await fetch(`${apiBase}/api/v1/coaching/recommendations`);
      return res.json();
    },
    triggerCorrection: async (pattern: number) => {
      const res = await fetch(`${apiBase}/api/v1/correction/trigger`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ haptic_pattern: pattern }),
      });
      return res.json();
    },
  };

  // WebSocket would update postureData and postureScore in real-time
  useEffect(() => {
    // Connect to WebSocket and update state on messages
  }, []);

  return (
    <PostureSyncContext.Provider value={{ postureData, postureScore, emgData, api }}>
      {children}
    </PostureSyncContext.Provider>
  );
}

export { PostureSyncContext };