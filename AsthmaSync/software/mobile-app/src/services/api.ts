/**
 * AsthmaSync — API Client Service
 * REST API client for the AsthmaSync backend.
 */

import AsyncStorage from '@react-native-async-storage/async-storage';

const DEFAULT_BASE_URL = 'https://api.asthmasync.io';

export interface RiskData {
  risk_score: number;
  risk_level: string;
  confidence: number;
  trend: string;
  contributing_factors: Array<{ factor: string; value: number; weight: number }>;
}

export interface AdherenceData {
  rescue_count_7d: number;
  rescue_count_30d: number;
  controller_adherence_pct: number;
  gina_controlled: boolean;
  last_rescue: string | null;
}

export interface TriggerData {
  trigger: string;
  contribution_pct: number;
  exposure_level: string;
  recommendation: string;
}

export interface EventData {
  timestamp: string;
  event_type: string;
  severity: number;
  message: string;
}

export interface ActionPlanData {
  zone: string;
  rescue_use_7d: number;
  last_spo2: number;
  steps: string[];
  last_updated: string;
}

export class ApiClient {
  private static instance: ApiClient;
  private baseUrl: string;

  private constructor() {
    this.baseUrl = DEFAULT_BASE_URL;
  }

  static getInstance(): ApiClient {
    if (!ApiClient.instance) {
      ApiClient.instance = new ApiClient();
    }
    return ApiClient.instance;
  }

  async setBaseUrl(url: string) {
    this.baseUrl = url;
    await AsyncStorage.setItem('api_base_url', url);
  }

  private async request<T>(path: string): Promise<T> {
    const url = `${this.baseUrl}${path}`;
    const response = await fetch(url, {
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Bearer ${await this.getToken()}`,
      },
    });
    if (!response.ok) {
      throw new Error(`API error: ${response.status} ${response.statusText}`);
    }
    return response.json();
  }

  private async getToken(): Promise<string> {
    return await AsyncStorage.getItem('auth_token') || '';
  }

  async getRisk(): Promise<RiskData> {
    return this.request('/api/v1/risk');
  }

  async getTriggers(): Promise<TriggerData[]> {
    return this.request('/api/v1/triggers');
  }

  async getAdherence(): Promise<AdherenceData> {
    return this.request('/api/v1/adherence');
  }

  async getEvents(limit: number = 50): Promise<EventData[]> {
    return this.request(`/api/v1/events?limit=${limit}`);
  }

  async getTrends(metric: string, hours: number = 24): Promise<any> {
    return this.request(`/api/v1/trends?metric=${metric}&hours=${hours}`);
  }

  async getActionPlan(): Promise<ActionPlanData> {
    return this.request('/api/v1/action-plan');
  }

  async getReport(): Promise<any> {
    return this.request('/api/v1/report');
  }

  async logEvent(eventType: string, value?: number, note?: string): Promise<void> {
    await fetch(`${this.baseUrl}/api/v1/event`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Bearer ${await this.getToken()}`,
      },
      body: JSON.stringify({
        event_type: eventType,
        value,
        note,
      }),
    });
  }
}