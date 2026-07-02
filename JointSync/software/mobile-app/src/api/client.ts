/**
 * JointSync Mobile App — API Client
 */

const API_BASE_URL = 'https://api.jointsync.cloud/api/v1';

export class ApiClient {
  private token: string | null = null;

  setToken(token: string) {
    this.token = token;
  }

  getToken(): string | null {
    return this.token;
  }

  private async request(path: string, options: RequestInit = {}) {
    const headers: Record<string, string> = {
      'Content-Type': 'application/json',
      ...(options.headers as Record<string, string>),
    };
    if (this.token) {
      headers['Authorization'] = `Bearer ${this.token}`;
    }

    const response = await fetch(`${API_BASE_URL}${path}`, {
      ...options,
      headers,
    });

    if (!response.ok) {
      throw new Error(`API error: ${response.status} ${response.statusText}`);
    }

    return response.json();
  }

  // Auth
  async login(email: string, password: string) {
    const formData = new URLSearchParams();
    formData.append('username', email);
    formData.append('password', password);
    const response = await fetch(`${API_BASE_URL}/auth/login`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: formData.toString(),
    });
    if (!response.ok) throw new Error('Login failed');
    const data = await response.json();
    this.setToken(data.access_token);
    return data;
  }

  async register(email: string, name: string, password: string, diagnosis: string) {
    return this.request('/auth/register', {
      method: 'POST',
      body: JSON.stringify({ email, name, password, diagnosis }),
    });
  }

  // Joints
  async getJoints() {
    return this.request('/joints');
  }

  async addJoint(jointType: string, side: string, tagId: number) {
    return this.request('/joints', {
      method: 'POST',
      body: JSON.stringify({ joint_type: jointType, side, tag_id: tagId }),
    });
  }

  async getROMHistory(jointId: string, hours: number = 24) {
    return this.request(`/joints/${jointId}/rom?hours=${hours}`);
  }

  async getTempHistory(jointId: string, hours: number = 24) {
    return this.request(`/joints/${jointId}/temperature?hours=${hours}`);
  }

  async getThermalScans(jointId: string) {
    return this.request(`/joints/${jointId}/thermal`);
  }

  // Flare prediction
  async getFlareRisk(jointId: string) {
    return this.request(`/joints/${jointId}/flare-risk`);
  }

  // Therapy
  async createTherapySession(jointId: string, mode: string, targetMmhg: number) {
    return this.request('/therapy/sessions', {
      method: 'POST',
      body: JSON.stringify({ joint_id: jointId, mode, target_mmhg: targetMmhg }),
    });
  }

  async getTherapySessions() {
    return this.request('/therapy/sessions');
  }

  // Reports
  async getClinicalReport() {
    return this.request('/reports/clinical');
  }

  // Health
  async checkHealth() {
    return this.request('/health');
  }
}

export const apiClient = new ApiClient();