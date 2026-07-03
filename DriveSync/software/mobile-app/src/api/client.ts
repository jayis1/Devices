/**
 * DriveSync API Client
 *
 * HTTP client for the DriveSync cloud backend.
 * License: MIT
 */

import axios from 'axios';
import AsyncStorage from '@react-native-async-storage/async-storage';

const API_BASE_URL = 'https://api.drivesync.cloud/api/v1';

const apiClient = axios.create({
  baseURL: API_BASE_URL,
  timeout: 10000,
  headers: { 'Content-Type': 'application/json' },
});

// Attach JWT token to all requests
apiClient.interceptors.request.use(async (config) => {
  const token = await AsyncStorage.getItem('drivesync_token');
  if (token) {
    config.headers.Authorization = `Bearer ${token}`;
  }
  return config;
});

export class ApiClient {
  static async login(email: string, password: string) {
    const formData = new URLSearchParams();
    formData.append('username', email);
    formData.append('password', password);
    const response = await apiClient.post('/auth/login', formData.toString(), {
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    });
    await AsyncStorage.setItem('drivesync_token', response.data.access_token);
    return response.data;
  }

  static async register(email: string, password: string, name: string) {
    const response = await apiClient.post('/auth/register', {
      email, password, name,
    });
    return response.data;
  }

  static async getTrips() {
    const response = await apiClient.get('/trips');
    return response.data;
  }

  static async getTrip(tripId: string) {
    const response = await apiClient.get(`/trips/${tripId}`);
    return response.data;
  }

  static async getTripEvents(tripId: string) {
    const response = await apiClient.get(`/trips/${tripId}/events`);
    return response.data;
  }

  static async getTripTimeline(tripId: string) {
    const response = await apiClient.get(`/trips/${tripId}/timeline`);
    return response.data;
  }

  static async getWeeklyCoaching() {
    const response = await apiClient.get('/coaching/weekly');
    return response.data;
  }

  static async pairDevice(hubId: string, wheelId?: string, beltId?: string, obdId?: string) {
    const response = await apiClient.post('/devices/pair', {
      hub_id: hubId,
      wheel_node_id: wheelId,
      belt_tag_id: beltId,
      obd_dongle_id: obdId,
    });
    return response.data;
  }

  static async setEmergencyContact(name: string, phone: string) {
    // Would need a PATCH endpoint — stub for now
    return { name, phone };
  }

  static async logout() {
    await AsyncStorage.removeItem('drivesync_token');
  }
}