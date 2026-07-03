/**
 * Auth Context — manages JWT auth state
 * License: MIT
 */

import React, { createContext, useContext, useState, useEffect } from 'react';
import { ApiClient } from '../api/client';
import AsyncStorage from '@react-native-async-storage/async-storage';

interface User {
  id: string;
  email: string;
  name: string;
}

interface AuthContextType {
  user: User | null;
  token: string | null;
  login: (email: string, password: string) => Promise<void>;
  register: (email: string, password: string, name: string) => Promise<void>;
  logout: () => Promise<void>;
  loading: boolean;
}

const AuthContext = createContext<AuthContextType>({
  user: null,
  token: null,
  login: async () => {},
  register: async () => {},
  logout: async () => {},
  loading: true,
});

export const useAuth = () => useContext(AuthContext);

export const AuthProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const [user, setUser] = useState<User | null>(null);
  const [token, setToken] = useState<string | null>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    // Check for stored token on app launch
    AsyncStorage.getItem('drivesync_token').then(async (storedToken) => {
      if (storedToken) {
        setToken(storedToken);
        try {
          // Fetch user profile
          // ApiClient.get('/patients/me') equivalent
          const userData = await AsyncStorage.getItem('drivesync_user');
          if (userData) setUser(JSON.parse(userData));
        } catch {
          await AsyncStorage.removeItem('drivesync_token');
        }
      }
      setLoading(false);
    });
  }, []);

  const login = async (email: string, password: string) => {
    await ApiClient.login(email, password);
    setToken(await AsyncStorage.getItem('drivesync_token'));
    const userData = { id: 'unknown', email, name: email.split('@')[0] };
    setUser(userData);
    await AsyncStorage.setItem('drivesync_user', JSON.stringify(userData));
  };

  const register = async (email: string, password: string, name: string) => {
    await ApiClient.register(email, password, name);
    await login(email, password);
  };

  const logout = async () => {
    await ApiClient.logout();
    setUser(null);
    setToken(null);
  };

  return (
    <AuthContext.Provider value={{ user, token, login, register, logout, loading }}>
      {children}
    </AuthContext.Provider>
  );
};