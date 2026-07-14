// Account persistence service.
//
// Persists/retrieves saved SMB login credentials in AsyncStorage under
// a single JSON-encoded key. Pure, no React state. Consumers should
// call load() once on mount and use the mutation helpers below.

import AsyncStorage from '@react-native-async-storage/async-storage';

const KEY = 'savedAccounts';

const safeArray = (raw) => {
  if (!raw) return [];
  try {
    const parsed = JSON.parse(raw);
    return Array.isArray(parsed) ? parsed : [];
  } catch {
    return [];
  }
};

const persist = async (accounts) => {
  await AsyncStorage.setItem(KEY, JSON.stringify(accounts));
};

export const AuthService = {
  /** Load all saved accounts. Returns [] on any failure. */
  async load() {
    try {
      const raw = await AsyncStorage.getItem(KEY);
      return safeArray(raw);
    } catch (err) {
      console.warn('AuthService.load failed:', err?.message);
      return [];
    }
  },

  /** Return only the accounts for a given server URL, newest first. */
  forServer(accounts, serverUrl) {
    return accounts.filter((a) => a.serverUrl === serverUrl).sort((a, b) => (b.lastUsed || 0) - (a.lastUsed || 0));
  },

  /**
   * Upsert account by (serverUrl, username). Returns the new accounts array.
   * Caller is responsible for surfacing errors.
   */
  async upsert(accounts, accountData) {
    const idx = accounts.findIndex((a) => a.serverUrl === accountData.serverUrl && a.username === accountData.username);
    let updated;
    if (idx >= 0) {
      updated = [...accounts];
      updated[idx] = { ...accountData, lastUsed: Date.now() };
    } else {
      updated = [...accounts, { ...accountData, lastUsed: Date.now() }];
    }
    await persist(updated);
    return updated;
  },

  /** Remove account by (serverUrl, username). Returns the new accounts array. */
  async remove(accounts, accountToRemove) {
    const updated = accounts.filter((a) => !(a.serverUrl === accountToRemove.serverUrl && a.username === accountToRemove.username));
    await persist(updated);
    return updated;
  },
};

export default AuthService;
