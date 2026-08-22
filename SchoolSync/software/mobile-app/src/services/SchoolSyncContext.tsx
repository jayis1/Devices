import React, { createContext, useContext, useMemo } from 'react';

const SchoolSyncContext = createContext({
  children: [
    { id: 'ava', readiness: 86, status: 'Ready' },
    { id: 'leo', readiness: 68, status: 'Missing lunch' },
  ],
});

export const SchoolSyncProvider = ({ children }: { children: React.ReactNode }) => {
  const value = useMemo(() => ({
    children: [
      { id: 'ava', readiness: 86, status: 'Ready' },
      { id: 'leo', readiness: 68, status: 'Missing lunch' },
    ],
  }), []);

  return <SchoolSyncContext.Provider value={value}>{children}</SchoolSyncContext.Provider>;
};

export const useSchoolSync = () => useContext(SchoolSyncContext);
