import React, { createContext, useContext } from 'react';

type Overview = {
  forecast: { expected_minutes: number; confidence: number; strategy: string };
  summary: string;
  decisions: { label: string; action: string; rationale: string }[];
};

const mockOverview: Overview = {
  forecast: { expected_minutes: 184, confidence: 0.82, strategy: 'staged_generator_window' },
  summary: 'Minimum cold hold time is 198 minutes; mean product temperature is -2.3°C.',
  decisions: [
    { label: 'Router', action: 'keep_on', rationale: 'communications preserved' },
    { label: 'TV Console', action: 'shed', rationale: 'reserve under 60 minutes' },
  ],
};

const Context = createContext<Overview>(mockOverview);
export const OutageSyncProvider = ({ children }: { children: React.ReactNode }) => (
  <Context.Provider value={mockOverview}>{children}</Context.Provider>
);
export const useOutageSync = () => useContext(Context);
