import { Text, TextInput, TouchableOpacity, View } from 'react-native';

import { loginStyles as styles } from './styles';

const ServerStep = ({ serverUrl, setServerUrl, isLoading, onSubmit }) => (
  <View style={styles.form}>
    <View style={styles.inputGroup}>
      <Text style={styles.label}>Server URL</Text>
      <TextInput
        style={styles.input}
        value={serverUrl}
        onChangeText={setServerUrl}
        placeholder="smb://hostname"
        autoCapitalize="none"
        autoCorrect={false}
        editable={!isLoading}
      />
    </View>

    <TouchableOpacity
      style={[styles.connectButton, isLoading && styles.connectButtonDisabled]}
      onPress={onSubmit}
      disabled={isLoading}>
      <Text style={styles.connectButtonText}>Continue</Text>
    </TouchableOpacity>
  </View>
);

export default ServerStep;
