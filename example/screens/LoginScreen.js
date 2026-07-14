import { useEffect, useRef, useState } from 'react';
import { Animated, KeyboardAvoidingView, Platform, SafeAreaView, ScrollView, Text, TouchableOpacity, View } from 'react-native';

import { AuthService } from '../services/AuthService';
import AccountsList from './login/AccountsList';
import NewAccountForm from './login/NewAccountForm';
import ServerStep from './login/ServerStep';
import { loginStyles as styles } from './login/styles';

const LoginScreen = ({ onLogin, isLoading, showAlert }) => {
  const [serverUrl, setServerUrl] = useState('smb://192.168.100.231');
  const [username, setUsername] = useState('user01');
  const [password, setPassword] = useState('1');
  const [step, setStep] = useState('server');
  const [savedAccounts, setSavedAccounts] = useState([]);
  const [showNewAccount, setShowNewAccount] = useState(false);
  const [showAllAccounts, setShowAllAccounts] = useState(false);

  const fadeAnim = useRef(new Animated.Value(0)).current;
  const openRowRef = useRef(null);

  useEffect(() => {
    (async () => {
      const accounts = await AuthService.load();
      setSavedAccounts(accounts);
    })();

    Animated.timing(fadeAnim, {
      toValue: 1,
      duration: 800,
      useNativeDriver: true,
    }).start();
  }, [fadeAnim]);

  useEffect(() => {
    if (step === 'credentials' && serverUrl) {
      const accountsForServer = savedAccounts.filter((a) => a.serverUrl === serverUrl);
      setShowNewAccount(accountsForServer.length === 0);
    }
  }, [step, serverUrl, savedAccounts]);

  const saveAccount = async (accountData) => {
    try {
      const updated = await AuthService.upsert(savedAccounts, accountData);
      setSavedAccounts(updated);
    } catch (error) {
      console.error('Error saving account:', error);
    }
  };

  const removeAccount = async (accountToRemove) => {
    try {
      const updated = await AuthService.remove(savedAccounts, accountToRemove);
      setSavedAccounts(updated);
    } catch (error) {
      console.error('Error removing account:', error);
    }
  };

  const handleServerSubmit = () => {
    if (!serverUrl.trim()) {
      showAlert('Error', 'Please enter a server URL');
      return;
    }
    setStep('credentials');
  };

  const handleQuickLogin = (account) => {
    setUsername(account.username);
    setPassword(account.password);
    saveAccount(account);
    onLogin(account);
  };

  const handleNewAccountLogin = () => {
    if (!serverUrl.trim() || !username.trim() || !password.trim()) {
      showAlert('Error', 'Please fill in all fields');
      return;
    }
    const accountData = { serverUrl, username, password };
    saveAccount(accountData);
    onLogin(accountData);
  };

  const accountsForServer = AuthService.forServer(savedAccounts, serverUrl);

  const sectionTitle = (() => {
    if (step === 'server') return 'Connect to Server';
    if (showNewAccount) return accountsForServer.length > 0 ? 'Add New Account' : 'Login Credentials';
    return showAllAccounts ? 'All Accounts' : 'Recent Account';
  })();

  const sectionSubtitle = step === 'server' ? 'Enter your SMB server URL' : 'Select a saved account or login with new credentials';

  const showSavedAccountsList = !showNewAccount && accountsForServer.length > 0;

  return (
    <SafeAreaView style={styles.container}>
      <KeyboardAvoidingView
        behavior={Platform.OS === 'ios' ? 'padding' : 'height'}
        style={styles.keyboardView}>
        {step === 'credentials' ? (
          <TouchableOpacity
            style={styles.backButtonTop}
            onPress={() => setStep('server')}>
            <Text style={styles.backButtonText}>{'\u2190 Back to Server'}</Text>
          </TouchableOpacity>
        ) : (
          <View style={styles.backButtonTop}>
            <Text style={styles.backButtonText} />
          </View>
        )}

        <ScrollView
          contentContainerStyle={styles.scrollContent}
          showsVerticalScrollIndicator={false}
          bounces={false}>
          <Animated.View style={[styles.content, { opacity: fadeAnim }]}>
            <View style={styles.header}>
              <Text style={styles.title}>{sectionTitle}</Text>
              <Text style={styles.subtitle}>{sectionSubtitle}</Text>
            </View>

            <View style={styles.formContainer}>
              {step === 'server' ? (
                <ServerStep
                  serverUrl={serverUrl}
                  setServerUrl={setServerUrl}
                  isLoading={isLoading}
                  onSubmit={handleServerSubmit}
                />
              ) : (
                <View style={styles.form}>
                  {showSavedAccountsList ? (
                    <AccountsList
                      accounts={accountsForServer}
                      showAllAccounts={showAllAccounts}
                      setShowAllAccounts={setShowAllAccounts}
                      isLoading={isLoading}
                      onSelect={handleQuickLogin}
                      onRemove={removeAccount}
                      openRowRef={openRowRef}
                      sectionTitle={sectionTitle}
                      onAddNewAccount={() => setShowNewAccount(true)}
                    />
                  ) : (
                    <NewAccountForm
                      sectionTitle={sectionTitle}
                      showCloseButton={accountsForServer.length > 0}
                      onCloseNewAccount={() => setShowNewAccount(false)}
                      username={username}
                      setUsername={setUsername}
                      password={password}
                      setPassword={setPassword}
                      isLoading={isLoading}
                      onSubmit={handleNewAccountLogin}
                    />
                  )}
                </View>
              )}
            </View>
          </Animated.View>
        </ScrollView>
      </KeyboardAvoidingView>
    </SafeAreaView>
  );
};

export default LoginScreen;
