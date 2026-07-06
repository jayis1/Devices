/**
 * SightSync Mobile App — API Client
 * License: MIT
 */

import axios from 'axios';
import AsyncStorage from '@react-native-async-storage/async-storage';

const BASE_URL = 'https://api.sightsync.cloud/v1';

const api = axios.create({
  baseURL: BASE_URL,
  timeout: 10000,
});

// Attach JWT token to requests
api.interceptors.request.use(async (config) => {
  const token = await AsyncStorage.getItem('@sightsync_token');
  if (token) {
    config.headers.Authorization = `Bearer ${token}`;
  }
  return config;
});

export default {
  // Health
  health: () => api.get('/health'),

  // Auth
  register: (email: string, password: string, name: string) =>
    api.post('/auth/register', { email, password, name }),
  login: (email: string, password: string) =>
    api.post('/auth/login', { email, password }),

  // Fatigue
  getCurrentFatigue: () => api.get('/fatigue/current'),
  getFatigueHistory: (days: number = 7) =>
    api.get('/fatigue/history', { params: { days } }),

  // Distance
  getDistanceHistory: (days: number = 1) =>
    api.get('/distance/history', { params: { days } }),

  // Blink
  getBlinkHistory: (days: number = 1) =>
    api.get('/blink/history', { params: { days } }),

  // Light
  getLightHistory: (days: number = 1) =>
    api.get('/light/history', { params: { days } }),

  // Myopia
  getMyopiaForecast: (childId?: string) =>
    api.get('/myopia/forecast', { params: { child_id: childId } }),

  // Reports
  getOptometristReport: (format: string = 'json') =>
    api.get('/report/optometrist', { params: { format } }),

  // Lamp
  getLampPolicy: () => api.get('/lamp/policy'),
  setLampOverride: (cct: number, brightness: number, durationMin: number = 30) =>
    api.post('/lamp/override', { cct, brightness, duration_min: durationMin }),
};