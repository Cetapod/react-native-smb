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

  const loadFiles = useCallback(
    async (path) => {
      setCurrentPath(path);
      setFilesList([]);
      setIsLoading(true);

      try {
        await ensureConnected();
        const files = await smb.listDirectory(path, false, -1).result();
        setFilesList(files || []);
      } catch (error) {
        if (path === '/' && currentUsername && isAccessDeniedError(error)) {
          try {
            const userPath = `/${currentUsername}`;
            const files = await smb.listDirectory(userPath, false, -1).result();
            setFilesList(files || []);
            setCurrentPath(userPath);
            setIsLoading(false);
            showAlert('Info', 'Root directory not accessible. Showing your home directory instead.');
            return;
          } catch {
            setIsLoading(false);
            handleSMBError(error, 'loadFiles', `directory "${path}"`, showAlert);
            return;
          }
        }
        setIsLoading(false);
        handleSMBError(error, 'loadFiles', `directory "${path}"`, showAlert);
      } finally {
        setIsLoading(false);
      }
    },
    [smb, currentUsername, ensureConnected, setIsLoading, showAlert],
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

  const handleBackToLogin = useCallback(async () => {
    if (!smb) return;
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
  }, [smb]);

  const handleBackToShares = useCallback(() => {
    setCurrentScreen('shares');
    setSelectedShare('');
    setFilesList([]);
    setCurrentPath('/');
  }, []);

  const value = useMemo(
    () => ({
      smb,
      moduleError,
      isLoading,
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
    [smb, moduleError, isLoading, isDisconnecting, setIsLoading, alertConfig, showAlert, showPrompt, hideAlert, currentScreen, sharesList, selectedShare, currentUsername, filesList, currentPath, loadFiles, loadShares, handleLogin, handleSelectShare, handleBackToLogin, handleBackToShares, ensureConnected, prefs, updatePrefs],
  );

  return <SmbContext.Provider value={value}>{children}</SmbContext.Provider>;
};

export const useSmb = () => {
  const ctx = useContext(SmbContext);
  if (!ctx) throw new Error('useSmb must be used inside <SmbProvider>');
  return ctx;
};