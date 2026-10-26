import AsyncStorage from '@react-native-async-storage/async-storage';
import { createContext, useCallback, useContext, useEffect, useMemo, useRef, useState } from 'react';

import { SMB } from '@cetapod/react-native-smb';

import { useCustomAlert } from '../hooks/useCustomAlert';
import { AuthService } from '../services/AuthService';
import { handleSMBError, isAccessDeniedError } from '../utils/errorHandler';

const SmbContext = createContext(null);

const PREFS_KEY = '@cetapod/prefs/v1';
const defaultPrefs = { sortBy: 'type', sortOrder: 'asc', showHidden: false };

export const SmbProvider = ({ children }) => {
  const [smb, setSmb] = useState(null);
  const [moduleError, setModuleError] = useState(null);
  const [isLoading, setIsLoading] = useState(false);
  /** Directory listing in progress (streaming); not used for login/share spinners. */
  const [isListing, setIsListing] = useState(false);
  const [isDisconnecting, setIsDisconnecting] = useState(false);

  useEffect(() => {
    try {
      setSmb(SMB());
    } catch (error) {
      setModuleError(error.message);
    }
  }, []);
  const { alertConfig, showAlert, showPrompt, hideAlert } = useCustomAlert();

  const [currentScreen, setCurrentScreen] = useState('login');
  const [sharesList, setSharesList] = useState([]);
  const [selectedShare, setSelectedShare] = useState('');
  const [currentUsername, setCurrentUsername] = useState('');
  const [filesList, setFilesList] = useState([]);
  const [currentPath, setCurrentPath] = useState('/');

  const [prefs, setPrefs] = useState(defaultPrefs);
  const prefsHydratedRef = useRef(false);

  useEffect(() => {
    let mounted = true;
    (async () => {
      try {
        const raw = await AsyncStorage.getItem(PREFS_KEY);
        if (raw && mounted) {
          const parsed = JSON.parse(raw);
          setPrefs((prev) => ({ ...prev, ...parsed }));
        }
      } catch {} finally {
        prefsHydratedRef.current = true;
      }
    })();
    return () => {
      mounted = false;
    };
  }, []);

  const updatePrefs = useCallback((patch) => {
    setPrefs((prev) => {
      const next = { ...prev, ...patch };
      AsyncStorage.setItem(PREFS_KEY, JSON.stringify(next)).catch(() => {});
      return next;
    });
  }, []);

  const lastCredsRef = useRef(null);
  const restoreAttemptedRef = useRef(false);

  const ensureConnected = useCallback(async () => {
    if (!smb) return false;
    if (smb.isInitialized()) return true;
    const creds = lastCredsRef.current;
    if (!creds) return false;
    try {
      await smb.initialize(creds.serverUrl, { username: creds.username, password: creds.password }).result();
      return smb.isInitialized();
    } catch {
      return false;
    }
  }, [smb]);

  // The listing currently feeding filesList; a newer load cancels and replaces it.
  const listTaskRef = useRef(null);
  const loadGenerationRef = useRef(0);

  const cancelListing = useCallback(() => {
    listTaskRef.current?.cancel();
    listTaskRef.current = null;
  }, []);

  // Streams entries into filesList as batches arrive. Resolves false when superseded.
  const streamDirectory = useCallback(
    async (path) => {
      cancelListing();
      setFilesList([]);
      const task = smb.listDirectory(path, false, -1);
      listTaskRef.current = task;
      const isCurrent = () => listTaskRef.current === task;

      let pending = [];
      let flushTimer = null;
      const flushPending = () => {
        if (!isCurrent() || pending.length === 0) return;
        const chunk = pending;
        pending = [];
        setFilesList((prev) => prev.concat(chunk));
      };
      const scheduleFlush = () => {
        if (flushTimer != null) return;
        flushTimer = setTimeout(() => {
          flushTimer = null;
          flushPending();
        }, 100);
      };

      const unsubscribe = task.subscribeEntries((batch) => {
        if (!isCurrent()) return;
        pending.push(...batch);
        scheduleFlush();
      });
      try {
        await task.wait();
        if (flushTimer != null) {
          clearTimeout(flushTimer);
          flushTimer = null;
        }
        flushPending();
        return isCurrent();
      } catch (error) {
        if (!isCurrent()) return false;
        throw error;
      } finally {
        unsubscribe();
        if (flushTimer != null) clearTimeout(flushTimer);
        if (isCurrent()) listTaskRef.current = null;
      }
    },
    [smb, cancelListing],
  );

  const loadFiles = useCallback(
    async (path) => {
      const generation = ++loadGenerationRef.current;
      const isCurrentLoad = () => loadGenerationRef.current === generation;
      cancelListing();
      setCurrentPath(path);
      setFilesList([]);
      setIsListing(true);

      try {
        await ensureConnected();
        if (!isCurrentLoad()) return;
        await streamDirectory(path);
      } catch (error) {
        if (!isCurrentLoad()) return;
        if (path === '/' && currentUsername && isAccessDeniedError(error)) {
          try {
            const userPath = `/${currentUsername}`;
            setCurrentPath(userPath);
            if (await streamDirectory(userPath)) {
              showAlert('Info', 'Root directory not accessible. Showing your home directory instead.');
            }
            return;
          } catch {
            if (isCurrentLoad()) handleSMBError(error, 'loadFiles', `directory "${path}"`, showAlert);
            return;
          }
        }
        handleSMBError(error, 'loadFiles', `directory "${path}"`, showAlert);
      } finally {
        if (isCurrentLoad()) setIsListing(false);
      }
    },
    [currentUsername, ensureConnected, cancelListing, streamDirectory, showAlert],
  );

  const checkAndNavigateToHomeDirectory = useCallback(
    async (username) => {
      try {
        const files = await smb.listDirectory('/', false, -1).result();
        const homeDirectory = files.find((file) => file.isDirectory && file.name.toLowerCase() === username.toLowerCase());

        if (homeDirectory) {
          await loadFiles(`/${homeDirectory.name}`);
          return true;
        }
        await loadFiles('/');
        return false;
      } catch {
        await loadFiles('/');
        return false;
      }
    },
    [smb, loadFiles],
  );

  const loadShares = useCallback(async () => {
    try {
      const shares = await smb.listShares().result();
      setSharesList(shares || []);
      setCurrentScreen('shares');
    } catch (error) {
      handleSMBError(error, 'listShares', 'server shares', showAlert);
    }
  }, [smb, showAlert]);

  const handleLogin = useCallback(
    async (credentials, { silent = false } = {}) => {
      if (!smb) {
        if (!silent) showAlert('Error', 'SMB client not available');
        return;
      }

      try {
        if (!silent) setIsLoading(true);
        await smb
          .initialize(credentials.serverUrl, {
            username: credentials.username,
            password: credentials.password,
          })
          .result();

        if (smb.isInitialized()) {
          lastCredsRef.current = credentials;
          setCurrentUsername(credentials.username);
          await loadShares();
        } else if (!silent) {
          showAlert('Initialization Failed', 'Could not initialize connection to server');
        }
      } catch (error) {
        if (!silent) {
          handleSMBError(error, 'login', credentials.serverUrl, showAlert);
        }
      } finally {
        if (!silent) setIsLoading(false);
      }
    },
    [smb, setIsLoading, showAlert, loadShares],
  );

  useEffect(() => {
    if (!smb || restoreAttemptedRef.current) return;
    restoreAttemptedRef.current = true;

    (async () => {
      try {
        const accounts = await AuthService.load();
        if (!accounts.length) return;
        const recent = [...accounts].sort((a, b) => (b.lastUsed || 0) - (a.lastUsed || 0))[0];
        if (!recent?.serverUrl || !recent?.username || !recent?.password) return;
        await handleLogin(recent, { silent: true });
      } catch {}
    })();
  }, [smb, handleLogin]);

  const handleSelectShare = useCallback(
    async (share) => {
      try {
        setIsLoading(true);
        const shareName = share.name || share;

        if (!(await ensureConnected())) {
          showAlert('Disconnected', 'Lost SMB session. Please log in again.');
          setCurrentScreen('login');
          return;
        }
        await smb.connectShare(shareName).result();
        setSelectedShare(shareName);

        await checkAndNavigateToHomeDirectory(currentUsername);
        setCurrentScreen('files');
      } catch (error) {
        const shareName = share.name || share;
        handleSMBError(error, 'connectShare', `share "${shareName}"`, showAlert);
      } finally {
        setIsLoading(false);
      }
    },
    [smb, setIsLoading, showAlert, ensureConnected, checkAndNavigateToHomeDirectory, currentUsername],
  );

  // Stop the visible listing and invalidate any load still in flight.
  const abandonListing = useCallback(() => {
    loadGenerationRef.current += 1;
    cancelListing();
  }, [cancelListing]);

  const handleBackToLogin = useCallback(async () => {
    if (!smb) return;
    abandonListing();
    setIsListing(false);
    setIsDisconnecting(true);
    try {
      await smb.disconnect().result();
    } catch {
    } finally {
      setIsDisconnecting(false);
    }
    lastCredsRef.current = null;
    setCurrentScreen('login');
    setSharesList([]);
    setSelectedShare('');
    setCurrentUsername('');
    setFilesList([]);
    setCurrentPath('/');
  }, [smb, abandonListing]);

  const handleBackToShares = useCallback(() => {
    abandonListing();
    setIsListing(false);
    setCurrentScreen('shares');
    setSelectedShare('');
    setFilesList([]);
    setCurrentPath('/');
  }, [abandonListing]);

  const value = useMemo(
    () => ({
      smb,
      moduleError,
      isLoading,
      isListing,
      isDisconnecting,
      setIsLoading,
      alertConfig,
      showAlert,
      showPrompt,
      hideAlert,
      currentScreen,
      setCurrentScreen,
      sharesList,
      selectedShare,
      currentUsername,
      filesList,
      currentPath,
      loadFiles,
      loadShares,
      handleLogin,
      handleSelectShare,
      handleBackToLogin,
      handleBackToShares,
      ensureConnected,
      prefs,
      updatePrefs,
    }),
    [smb, moduleError, isLoading, isListing, isDisconnecting, setIsLoading, alertConfig, showAlert, showPrompt, hideAlert, currentScreen, sharesList, selectedShare, currentUsername, filesList, currentPath, loadFiles, loadShares, handleLogin, handleSelectShare, handleBackToLogin, handleBackToShares, ensureConnected, prefs, updatePrefs],
  );

  return <SmbContext.Provider value={value}>{children}</SmbContext.Provider>;
};

export const useSmb = () => {
  const ctx = useContext(SmbContext);
  if (!ctx) throw new Error('useSmb must be used inside <SmbProvider>');
  return ctx;
};