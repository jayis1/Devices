import React, { createContext, useContext } from 'react';

const MoldSyncContext = createContext({
  moldRiskScore: 78,
  activeAlerts: 2,
  driestRoom: 'Office',
  highestRiskRoom: 'Bathroom East',
});

export const MoldSyncProvider = MoldSyncContext.Provider;
export const useMoldSync = () => useContext(MoldSyncContext);
