/**
 * ApiClient — REST + WebSocket client for CardioSync backend
 *
 * License: MIT
 */
import React, { createContext, useContext } from 'react';
import axios from 'axios';
import AsyncStorage from '@react-native-async-storage/async-storage';

const API_BASE = 'http://localhost:8000/api/v1';

const ApiContext = createContext(null);

export const ApiProvider = ({ children }) => {
  const [token, setToken] = React.useState(null);

  React.useEffect(() => {
    AsyncStorage.getItem('cardiosync_token').then(t => {
      if (t) setToken(t);
    });
  }, []);

  const client = axios.create({
    baseURL: API_BASE,
    headers: token ? { Authorization: `Bearer ${token}` } : {},
  });

  const api = {
    login: async (username, password) => {
      const formData = new URLSearchParams();
      formData.append('username', username);
      formData.append('password', password);
      const res = await axios.post(`${API_BASE}/auth/login`, formData, {
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      });
      setToken(res.data.access_token);
      await AsyncStorage.setItem('cardiosync_token', res.data.access_token);
      return res.data;
    },

    register: async (username, email, password) => {
      const res = await axios.post(`${API_BASE}/auth/register`, {
        username, email, password,
      });
      return res.data;
    },

    getDashboard: async () => {
      const res = await client.get('/dashboard');
      return res.data;
    },

    getECGEvents: async (limit = 50, offset = 0) => {
      const res = await client.get('/ecg/events', { params: { limit, offset } });
      return res.data;
    },

    getECGEventDetail: async (eventId) => {
      const res = await client.get(`/ecg/events/${eventId}`);
      return res.data;
    },

    getBPTrends: async (days = 7) => {
      const res = await client.get('/bp/trends', { params: { days } });
      return res.data;
    },

    getBPRecords: async (limit = 50) => {
      const res = await client.get('/bp/records', { params: { limit } });
      return res.data;
    },

    getHRVTrends: async (days = 7) => {
      const res = await client.get('/hrv/trends', { params: { days } });
      return res.data;
    },

    getStrokeRisk: async () => {
      const res = await client.get('/risk/stroke');
      return res.data;
    },

    getMonthlyReport: async () => {
      const res = await client.get('/reports/monthly');
      return res.data;
    },

    getEmergencyContacts: async () => {
      const res = await client.get('/alerts/contacts');
      return res.data;
    },

    setEmergencyContacts: async (contact1, contact2) => {
      const res = await client.post('/alerts/contacts', {
        contact_1: contact1, contact_2: contact2,
      });
      return res.data;
    },

    setBPSchedule: async (schedule) => {
      const res = await client.post('/config/bp-schedule', schedule);
      return res.data;
    },

    connectECGStream: () => {
      return new WebSocket(`ws://localhost:8000/api/v1/ecg/stream`);
    },
  };

  return <ApiContext.Provider value={api}>{children}</ApiContext.Provider>;
};

export const useApi = () => useContext(ApiContext);