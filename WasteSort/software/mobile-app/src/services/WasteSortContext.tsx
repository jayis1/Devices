import React, { createContext, useContext, useMemo, useState } from 'react';

type BinState = {
  id: string;
  stream: string;
  fillPct: number;
  odor: number;
};

type WasteSortContextValue = {
  bins: BinState[];
  diversionScore: number;
  setBins: React.Dispatch<React.SetStateAction<BinState[]>>;
};

const WasteSortContext = createContext<WasteSortContextValue | undefined>(undefined);

export const WasteSortProvider = ({ children }: { children: React.ReactNode }) => {
  const [bins, setBins] = useState<BinState[]>([
    { id: 'recycle', stream: 'Recycling', fillPct: 88, odor: 18 },
    { id: 'compost', stream: 'Compost', fillPct: 61, odor: 71 },
    { id: 'trash', stream: 'Landfill', fillPct: 43, odor: 22 },
  ]);

  const value = useMemo(
    () => ({
      bins,
      diversionScore: 72,
      setBins,
    }),
    [bins]
  );

  return <WasteSortContext.Provider value={value}>{children}</WasteSortContext.Provider>;
};

export const useWasteSort = () => {
  const ctx = useContext(WasteSortContext);
  if (!ctx) {
    throw new Error('useWasteSort must be used inside WasteSortProvider');
  }
  return ctx;
};
