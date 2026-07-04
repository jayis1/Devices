/**
 * GlucoSync Auth Context
 *
 * Manages user authentication state and JWT token storage.
 * License: MIT
 */

import React, { createContext, useContext, useState, useEffect } from 'react';
import AsyncStorage from '@react-native-async-storage/async-storage';

interface AuthState {
  userId: string | null;
  isAuthenticated: boolean;
  isLoading: boolean;
}

interface AuthContextValue extends AuthState {
  login: (userId: string, token: string) => Promise<void>;
  logout: () => Promise<void>;
}

const AuthContext = createContext<AuthContextValue | undefined>(undefined);

export function AuthProvider({ children }: { children: React.ReactNode }) {
  const [state, setState] = useState<AuthState>({
    userId: null,
    isAuthenticated: false,
    isLoading: true,
  });

  useEffect(() => {
    // Load stored credentials on app start
    AsyncStorage.getItem('glucosync_user_id').then((userId) => {
      AsyncStorage.getItem('glucosync_token').then((token) => {
        setState({
          userId: userId,
          isAuthenticated: !!userId && !!token,
          isLoading: false,
        });
      });
    });
  }, []);

  const login = async (userId: string, token: string) => {
    await AsyncStorage.setItem('glucosync_user_id', userId);
    await AsyncStorage.setItem('glucosync_token', token);
    setState({ userId, isAuthenticated: true, isLoading: false });
  };

  const logout = async () => {
    await AsyncStorage.removeItem('glucosync_user_id');
    await AsyncStorage.removeItem('glucosync_token');
    setState({ userId: null, isAuthenticated: false, isLoading: false });
  };

  return (
    <AuthContext.Provider value={{ ...state, login, logout }}>
      {children}
    </AuthContext.Provider>
  );
}

export function useAuth() {
  const ctx = useContext(AuthContext);
  if (!ctx) throw new Error('useAuth must be used within AuthProvider');
  return ctx;
}