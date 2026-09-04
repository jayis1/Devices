import React, { createContext, useContext } from 'react';

const demoState = {
  state: 'watch',
  risks: { contamination: 0.41, pumpFailure: 0.28, dryWell: 0.18, treatment: 0.33 },
  latestPh: 6.48,
  latestTurbidity: 2.2,
  pressureKpa: 410,
};

const WellSyncContext = createContext(demoState);

export const WellSyncProvider = ({ children }: { children: React.ReactNode }) => (
  <WellSyncContext.Provider value={demoState}>{children}</WellSyncContext.Provider>
);

export const useWellSync = () => useContext(WellSyncContext);
