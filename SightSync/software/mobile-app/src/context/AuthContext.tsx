/**
 * SightSync Mobile App — Auth Context
 * License: MIT
 */

import React, { createContext, useContext, useState, useEffect } from 'react';
import AsyncStorage from '@react-native-async-storage/async-storage';
import api from '../api/client';

interface AuthState {
  token: string | null;
  userId: string | null;
  isLoading: boolean;
  login: (email: string, password: string) => Promise<void>;
  register: (email: string, password: string, name: string) => Promise<void>;
  logout: () => Promise<void>;
}

const AuthContext = createContext<AuthState | undefined>(undefined);

export function AuthProvider({ children }: { children: React.ReactNode }) {
  const [token, setToken] = useState<string | null>(null);
  const [userId, setUserId] = useState<string | null>(null);
  const [isLoading, setIsLoading] = useState(true);

  useEffect(() => {
    // Load token from storage on app start
    AsyncStorage.getItem('@sightsync_token').then((stored) => {
      if (stored) setToken(stored);
      setIsLoading(false);
    });
  }, []);

  const login = async (email: string, password: string) => {
    const res = await api.login(email, password);
    const { token: newToken, user_id } = res.data;
    setToken(newToken);
    setUserId(user_id);
    await AsyncStorage.setItem('@sightsync_token', newToken);
  };

  const register = async (email: string, password: string, name: string) => {
    const res = await api.register(email, password, name);
    const { token: newToken, user_id } = res.data;
    setToken(newToken);
    setUserId(user_id);
    await AsyncStorage.setItem('@sightsync_token', newToken);
  };

  const logout = async () => {
    setToken(null);
    setUserId(null);
    await AsyncStorage.removeItem('@sightsync_token');
  };

  return (
    <AuthContext.Provider value={{ token, userId, isLoading, login, register, logout }}>
      {children}
    </AuthContext.Provider>
  );
}

export function useAuth() {
  const ctx = useContext(AuthContext);
  if (!ctx) throw new Error('useAuth must be used within AuthProvider');
  return ctx;
}