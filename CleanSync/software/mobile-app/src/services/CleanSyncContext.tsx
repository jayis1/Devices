import React, { createContext, useContext, useMemo, useState } from 'react';

type Overview = {
  cleanlinessScore: number;
  activeAlerts: string[];
};

const CleanSyncContext = createContext<Overview>({ cleanlinessScore: 91, activeAlerts: [] });

export function CleanSyncProvider({ children }: { children: React.ReactNode }) {
  const [cleanlinessScore] = useState(91);
  const [activeAlerts] = useState<string[]>(['Wet-floor risk in kitchen']);
  const value = useMemo(() => ({ cleanlinessScore, activeAlerts }), [cleanlinessScore, activeAlerts]);
  return <CleanSyncContext.Provider value={value}>{children}</CleanSyncContext.Provider>;
}

export function useCleanSync() {
  return useContext(CleanSyncContext);
}
