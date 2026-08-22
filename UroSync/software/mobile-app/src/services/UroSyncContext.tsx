import React, { createContext, useContext } from 'react';

const sampleState = {
  hydrationRisk: 0.62,
  utiRisk: 0.31,
  fallRisk: 0.44,
  nightlyVoids: [1, 1, 2, 2, 3, 2, 2],
  dailyIntakeMl: 1480,
  stripCartridgeRemaining: 34,
};

const UroSyncContext = createContext(sampleState);

export const UroSyncProvider = ({ children }: { children: React.ReactNode }) => {
  return <UroSyncContext.Provider value={sampleState}>{children}</UroSyncContext.Provider>;
};

export const useUroSync = () => useContext(UroSyncContext);
