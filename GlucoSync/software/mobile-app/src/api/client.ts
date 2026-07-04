/**
 * GlucoSync API Client
 *
 * Handles all communication with the GlucoSync Cloud backend.
 * License: MIT
 */

import axios from 'axios';
import AsyncStorage from '@react-native-async-storage/async-storage';

const API_BASE_URL = 'http://localhost:8000/api';

const api = axios.create({
  baseURL: API_BASE_URL,
  timeout: 10000,
  headers: { 'Content-Type': 'application/json' },
});

// Attach JWT token to all requests
api.interceptors.request.use(
  async (config) => {
    const token = await AsyncStorage.getItem('glucosync_token');
    if (token) {
      config.headers.Authorization = `Bearer ${token}`;
    }
    return config;
  },
  (error) => Promise.reject(error)
);

export interface GlucoseReading {
  user_id: string;
  glucose_mgdl: number;
  trend: number;
  forecast_30: number;
  forecast_60: number;
  hypo_risk: number;
  risk_score: number;
  iob: number;
  cob: number;
  hr: number;
  activity: number;
  created_at: string;
}

export interface MealEntry {
  food_class_id: number;
  food_confidence: number;
  carb_grams: number;
  portion_grams: number;
  glycemic_index: number;
  created_at: string;
}

export interface InsulinEntry {
  pen_type: number;
  pen_id: number;
  estimated_units: number;
  confidence: number;
  injection_dur_ms: number;
  created_at: string;
}

export interface TIRResult {
  tir_pct: number;
  below_pct: number;
  above_pct: number;
  avg_glucose: number;
  gmi: number;
  readings: number;
  days: number;
}

export const GlucoSyncAPI = {
  async getGlucose(userId: string, hours = 24): Promise<GlucoseReading[]> {
    const res = await api.get(`/glucose/${userId}?hours=${hours}`);
    return res.data;
  },

  async getMeals(userId: string, hours = 24): Promise<MealEntry[]> {
    const res = await api.get(`/meals/${userId}?hours=${hours}`);
    return res.data;
  },

  async getInsulin(userId: string, hours = 24): Promise<InsulinEntry[]> {
    const res = await api.get(`/insulin/${userId}?hours=${hours}`);
    return res.data;
  },

  async getTimeInRange(userId: string, days = 14): Promise<TIRResult> {
    const res = await api.get(`/analytics/tir/${userId}?days=${days}`);
    return res.data;
  },

  async getAGP(userId: string, days = 14) {
    const res = await api.get(`/analytics/agp/${userId}?days=${days}`);
    return res.data;
  },

  async getInsulinSensitivity(userId: string) {
    const res = await api.get(`/analytics/sensitivity/${userId}`);
    return res.data;
  },

  async register(email: string, password: string, diabetesType: string, weightKg: number) {
    const res = await api.post('/register', {
      email,
      password,
      diabetes_type: diabetesType,
      weight_kg: weightKg,
    });
    await AsyncStorage.setItem('glucosync_user_id', res.data.user_id);
    return res.data;
  },

  async addEmergencyContact(userId: string, name: string, phone: string, relationship: string) {
    const res = await api.post(`/contacts/${userId}`, { name, phone, relationship });
    return res.data;
  },

  async getEmergencyContacts(userId: string) {
    const res = await api.get(`/contacts/${userId}`);
    return res.data;
  },
};