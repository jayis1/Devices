import React, { createContext, useContext, useState, useEffect, ReactNode } from 'react';
import WebSocketService from './WebSocketService';

interface RideData {
  speed_kmh: number;
  cadence_rpm: number;
  tire_pressure: number;
  gps_lat: number;
  gps_lon: number;
  heading: number;
  battery: number;
}

interface LockData {
  lock_state: number;
  gps_lat: number;
  gps_lon: number;
  battery: number;
}

interface CycleGuardState {
  ride: RideData | null;
  lock: LockData | null;
  collisionRisk: number;
  blindspotClass: number;
  crashAlert: boolean;
  theftAlert: boolean;
  connected: boolean;
  armLock: () => void;
  disarmLock: () => void;
  cancelCrashAlert: () => void;
}

const CycleGuardContext = createContext<CycleGuardState | undefined>(undefined);

const LOCK_STATES: Record<number, string> = {
  0: 'Disarmed', 1: 'Armed', 2: 'Tamper', 3: 'Alarm', 4: 'Tracking',
};

export function CycleGuardProvider({ children }: { children: ReactNode }) {
  const [ride, setRide] = useState<RideData | null>(null);
  const [lock, setLock] = useState<LockData | null>(null);
  const [collisionRisk, setCollisionRisk] = useState(0);
  const [blindspotClass, setBlindspotClass] = useState(0);
  const [crashAlert, setCrashAlert] = useState(false);
  const [theftAlert, setTheftAlert] = useState(false);
  const [connected, setConnected] = useState(false);
  const [ws] = useState(() => new WebSocketService());

  useEffect(() => {
    ws.connect('wss://api.cycleguard.cloud/ws/realtime');
    ws.onMessage((data) => {
      if (data.speed_kmh !== undefined) {
        setRide({
          speed_kmh: data.speed_kmh,
          cadence_rpm: data.cadence_rpm || 0,
          tire_pressure: data.tire_pressure || 0,
          gps_lat: data.gps_lat || 0,
          gps_lon: data.gps_lon || 0,
          heading: data.heading || 0,
          battery: data.battery || 0,
        });
        setCollisionRisk(data.collision || 0);
        setBlindspotClass(data.bs_class || 0);
      }
      if (data.lock_state !== undefined) {
        setLock({
          lock_state: data.lock_state,
          gps_lat: data.gps_lat || 0,
          gps_lon: data.gps_lon || 0,
          battery: data.battery || 0,
        });
        if (data.lock_state >= 3) {
          setTheftAlert(true);
        }
      }
      if (data.crash === true) {
        setCrashAlert(true);
      }
    });
    ws.onOpen(() => setConnected(true));
    ws.onClose(() => setConnected(false));
    return () => ws.disconnect();
  }, [ws]);

  const armLock = () => {
    ws.send({ type: 'lock_arm' });
  };

  const disarmLock = () => {
    ws.send({ type: 'lock_disarm' });
  };

  const cancelCrashAlert = () => {
    setCrashAlert(false);
    ws.send({ type: 'crash_cancel' });
  };

  return (
    <CycleGuardContext.Provider value={{
      ride, lock, collisionRisk, blindspotClass,
      crashAlert, theftAlert, connected,
      armLock, disarmLock, cancelCrashAlert,
    }}>
      {children}
    </CycleGuardContext.Provider>
  );
}

export function useCycleGuard() {
  const ctx = useContext(CycleGuardContext);
  if (!ctx) throw new Error('useCycleGuard must be inside CycleGuardProvider');
  return ctx;
}