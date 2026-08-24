import React, { createContext, useContext, useMemo, useState } from 'react';

type Alert = { type: string; message: string };

type ContextValue = {
  alerts: Alert[];
  addAlert: (alert: Alert) => void;
};

const Ctx = createContext<ContextValue | undefined>(undefined);

export const FoodAllergySyncProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const [alerts, setAlerts] = useState<Alert[]>([]);
  const value = useMemo(() => ({
    alerts,
    addAlert: (alert: Alert) => setAlerts((prev) => [...prev, alert]),
  }), [alerts]);
  return <Ctx.Provider value={value}>{children}</Ctx.Provider>;
};

export function useFoodAllergySync() {
  const ctx = useContext(Ctx);
  if (!ctx) throw new Error('FoodAllergySyncProvider missing');
  return ctx;
}
