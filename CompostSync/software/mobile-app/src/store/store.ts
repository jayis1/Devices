import { create } from 'zustand';

interface CompostStatus {
  device_id: string;
  phase: string;
  maturity_score: number;
  cn_ratio: number;
  days_to_ready: number;
  recommendation: string;
  mass_kg: number;
  diverted_kg: number;
}

interface Telemetry {
  temp_c: number[];
  moisture_pct: number[];
  co2_ppm: number;
  methane_ppm: number;
  mass_grams: number;
  battery_pct: number;
  phase: string;
}

interface Action {
  id: string;
  icon: string;
  title: string;
  description: string;
  time: string;
  completed: boolean;
}

interface Store {
  compostStatus: CompostStatus | null;
  telemetry: Telemetry | null;
  actions: Action[];
  device: { id: string; bin_volume_liters: number; compost_type: string } | null;
  bleConnected: boolean;
  loading: boolean;

  fetchStatus: () => Promise<void>;
  completeAction: (id: string) => void;
  bleConnect: () => void;
}

export const useStore = create<Store>((set) => ({
  compostStatus: null,
  telemetry: null,
  actions: [
    { id: '1', icon: '🔄', title: 'Turn the pile', description: 'Aerate to prevent anaerobic conditions', time: 'Due today', completed: false },
    { id: '2', icon: '🍃', title: 'Add dry leaves', description: 'C:N ratio is 22:1, add carbon', time: 'Today', completed: false },
    { id: '3', icon: '💧', title: 'Add water', description: 'Moisture is at 28%, needs moistening', time: 'Tomorrow', completed: false },
  ],
  device: { id: 'compost-hub-001', bin_volume_liters: 200, compost_type: 'hot' },
  bleConnected: false,
  loading: false,

  fetchStatus: async () => {
    set({ loading: true });
    try {
      // In production: call API or read BLE
      const mockStatus: CompostStatus = {
        device_id: 'compost-hub-001',
        phase: 'thermophilic',
        maturity_score: 35,
        cn_ratio: 28,
        days_to_ready: 42,
        recommendation: '🔥 Thermophilic phase! Temp 58.2°C. Turn in 3-5 days.',
        mass_kg: 12.5,
        diverted_kg: 47.3,
      };
      const mockTelemetry: Telemetry = {
        temp_c: [552, 582, 541],
        moisture_pct: [48, 55, 62],
        co2_ppm: 3200,
        methane_ppm: 45,
        mass_grams: 12500,
        battery_pct: 87,
        phase: 'thermophilic',
      };
      set({ compostStatus: mockStatus, telemetry: mockTelemetry, loading: false });
    } catch (e) {
      set({ loading: false });
    }
  },

  completeAction: (id) => {
    set((state) => ({
      actions: state.actions.map((a) => a.id === id ? { ...a, completed: true } : a),
    }));
  },

  bleConnect: () => {
    // In production: use react-native-ble-plx to connect to hub
    set({ bleConnected: true });
  },
}));