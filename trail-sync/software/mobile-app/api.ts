/**
 * TrailSync API Client
 *
 * REST + WebSocket client for the TrailSync cloud dashboard.
 * SPDX-License-Identifier: MIT
 */
import axios from 'axios';

const BASE_URL = 'http://localhost:8023/api/v1';

export const api = {
  getBaseUrl: () => BASE_URL,

  // Runner telemetry
  postTelemetry: async (runnerId: string, data: any) => {
    const res = await axios.post(`${BASE_URL}/runners/${runnerId}/telemetry`, data);
    return res.data;
  },

  getRunner: async (runnerId: string) => {
    const res = await axios.get(`${BASE_URL}/runners/${runnerId}`);
    return res.data;
  },

  listRunners: async () => {
    const res = await axios.get(`${BASE_URL}/runners`);
    return res.data;
  },

  // Gait data
  postGait: async (runnerId: string, data: any) => {
    const res = await axios.post(`${BASE_URL}/runners/${runnerId}/gait`, data);
    return res.data;
  },

  // Injury risk
  getInjuryRisk: async (runnerId: string, days: number = 7) => {
    const res = await axios.get(`${BASE_URL}/runners/${runnerId}/injury_risk?days=${days}`);
    return res.data;
  },

  // Beacon conditions
  listBeacons: async () => {
    const res = await axios.get(`${BASE_URL}/beacons`);
    return res.data;
  },

  getBeacon: async (beaconId: string) => {
    const res = await axios.get(`${BASE_URL}/beacons/${beaconId}`);
    return res.data;
  },

  // SOS
  postSOS: async (data: any) => {
    const res = await axios.post(`${BASE_URL}/sos`, data);
    return res.data;
  },

  listSOS: async () => {
    const res = await axios.get(`${BASE_URL}/sos`);
    return res.data;
  },

  // Trails
  listTrails: async () => {
    const res = await axios.get(`${BASE_URL}/trails`);
    return res.data;
  },

  // Training sessions
  createSession: async (runnerId: string, data: any) => {
    const res = await axios.post(`${BASE_URL}/runners/${runnerId}/sessions`, data);
    return res.data;
  },

  listSessions: async (runnerId: string, limit: number = 20) => {
    const res = await axios.get(`${BASE_URL}/runners/${runnerId}/sessions?limit=${limit}`);
    return res.data;
  },
};