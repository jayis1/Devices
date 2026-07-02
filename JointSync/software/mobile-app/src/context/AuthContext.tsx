/**
 * JointSync Mobile App — Auth Context
 */

import React, { createContext, useContext, useState, useEffect } from 'react';
import { apiClient } from '../api/client';
import AsyncStorage from '@react-native-async-storage/async-storage';

interface User {
  id: string;
  email: string;
  name: string;
  diagnosis: string;
}

interface AuthContextType {
  user: User | null;
  token: string | null;
  login: (email: string, password: string) => Promise<void>;
  register: (email: string, name: string, password: string, diagnosis: string) => Promise<void>;
  logout: () => void;
}

const AuthContext = createContext<AuthContextType | null>(null);

export function AuthProvider({ children }: { children: React.ReactNode }) {
  const [user, setUser] = useState<User | null>(null);
  const [token, setToken] = useState<string | null>(null);

  useEffect(() => {
    // Restore session from storage
    AsyncStorage.getItem('token').then((stored) => {
      if (stored) {
        setToken(stored);
        apiClient.setToken(stored);
      }
    });
  }, []);

  const login = async (email: string, password: string) => {
    const data = await apiClient.login(email, password);
    setToken(data.access_token);
    apiClient.setToken(data.access_token);
    await AsyncStorage.setItem('token', data.access_token);
    // Fetch user profile
    const profile = await apiClient.request('/patients/me');
    setUser(profile);
  };

  const register = async (email: string, name: string, password: string, diagnosis: string) => {
    const user = await apiClient.register(email, name, password, diagnosis);
    await login(email, password);
  };

  const logout = () => {
    setToken(null);
    setUser(null);
    AsyncStorage.removeItem('token');
  };

  return (
    <AuthContext.Provider value={{ user, token, login, register, logout }}>
      {children}
    </AuthContext.Provider>
  );
}

export function useAuth() {
  const ctx = useContext(AuthContext);
  if (!ctx) throw new Error('useAuth must be used within AuthProvider');
  return ctx;
}