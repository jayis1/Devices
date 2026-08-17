import React, { createContext, useContext, useState, useEffect, ReactNode } from 'react';
import WebSocketService from './WebSocketService';

interface TremorData {
  tremor_class: number;
  tremor_amplitude: number;
  bradykinesia: number;
  onoff_state: number;
  hr: number;
}

interface GaitData {
  stride_length: number;
  cadence: number;
  freeze_index: number;
  fog_detected: boolean;
  festination: boolean;
}

interface TremorSyncState {
  tremor: TremorData | null;
  gait: GaitData | null;
  onoffState: number;
  fallRiskScore: number;
  nextDoseMin: number;
  fogActive: boolean;
  connected: boolean;
  triggerCueing: () => void;
  logManualDose: () => void;
}

const TremorSyncContext = createContext<TremorSyncState | undefined>(undefined);

export function TremorSyncProvider({ children }: { children: ReactNode }) {
  const [tremor, setTremor] = useState<TremorData | null>(null);
  const [gait, setGait] = useState<GaitData | null>(null);
  const [onoffState, setOnoffState] = useState(0);
  const [fallRiskScore, setFallRiskScore] = useState(0);
  const [nextDoseMin, setNextDoseMin] = useState(180);
  const [fogActive, setFogActive] = useState(false);
  const [connected, setConnected] = useState(false);
  const [ws] = useState(() => new WebSocketService());

  useEffect(() => {
    ws.connect('wss://api.tremorsync.cloud/ws/realtime');
    ws.onMessage((data) => {
      if (data.tremor_class !== undefined) {
        setTremor({
          tremor_class: data.tremor_class,
          tremor_amplitude: data.tremor_amplitude || 0,
          bradykinesia: data.bradykinesia || 0,
          onoff_state: data.onoff_state || 0,
          hr: data.hr || 0,
        });
        setOnoffState(data.onoff_state || 0);
      }
    });
    ws.onOpen(() => setConnected(true));
    ws.onClose(() => setConnected(false));
    return () => ws.disconnect();
  }, [ws]);

  const triggerCueing = () => {
    ws.send({ type: 'cueing_trigger', pattern: 'metronome_100' });
  };

  const logManualDose = () => {
    ws.send({ type: 'manual_dose' });
    setNextDoseMin(240);
  };

  return (
    <TremorSyncContext.Provider value={{
      tremor, gait, onoffState, fallRiskScore, nextDoseMin, fogActive, connected,
      triggerCueing, logManualDose
    }}>
      {children}
    </TremorSyncContext.Provider>
  );
}

export function useTremorSync() {
  const ctx = useContext(TremorSyncContext);
  if (!ctx) throw new Error('useTremorSync must be inside TremorSyncProvider');
  return ctx;
}