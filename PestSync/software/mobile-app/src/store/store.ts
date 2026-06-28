/**
 * PestSync Redux Store (Zustand-style with Redux Toolkit)
 * software/mobile-app/src/store/store.ts
 */
import { configureStore, createSlice } from '@reduxjs/toolkit';

interface AppState {
  isConnected: boolean;
  token: string | null;
  currentScreen: string;
  alerts: any[];
  detections: any[];
}

const initialState: AppState = {
  isConnected: false,
  token: null,
  currentScreen: 'Dashboard',
  alerts: [],
  detections: [],
};

const appSlice = createSlice({
  name: 'app',
  initialState,
  reducers: {
    setConnected: (state, action) => { state.isConnected = action.payload; },
    setToken: (state, action) => { state.token = action.payload; },
    setScreen: (state, action) => { state.currentScreen = action.payload; },
    setAlerts: (state, action) => { state.alerts = action.payload; },
    addAlert: (state, action) => { state.alerts.unshift(action.payload); },
    setDetections: (state, action) => { state.detections = action.payload; },
  },
});

export const { setConnected, setToken, setScreen, setAlerts, addAlert, setDetections } =
  appSlice.actions;

export const store = configureStore({
  reducer: appSlice.reducer,
});