import { ActivityIndicator, Text, TextInput, TouchableOpacity, View } from 'react-native';

import { loginStyles as styles } from './styles';

// New-account credentials form. Used as the only choice when no accounts
// exist for the chosen server, or as a sub-form when the user opts to add
// one alongside existing accounts.

const NewAccountForm = ({ sectionTitle, showCloseButton, onCloseNewAccount, username, setUsername, password, setPassword, isLoading, onSubmit }) => (
  <View style={styles.newAccountSection}>
    {showCloseButton && (
      <View style={styles.newAccountModalHeader}>
        <Text style={styles.sectionTitle}>{sectionTitle}</Text>
        <TouchableOpacity
          style={styles.closeButton}
          onPress={onCloseNewAccount}>
          <Text style={styles.closeButtonText}>{'\u00D7'}</Text>
        </TouchableOpacity>
      </View>
    )}

    <View style={styles.newAccountForm}>
      <View style={styles.inputGroup}>
        <Text style={styles.label}>Username</Text>
        <TextInput
          style={styles.input}
          value={username}
          onChangeText={setUsername}
          placeholder="Enter username"
          autoCapitalize="none"
          autoCorrect={false}
          editable={!isLoading}
        />
      </View>

      <View style={styles.inputGroup}>
        <Text style={styles.label}>Password</Text>
        <TextInput
          style={styles.input}
          value={password}
          onChangeText={setPassword}
          placeholder="Enter password"
          secureTextEntry
          editable={!isLoading}
        />
      </View>

      <TouchableOpacity
        style={[styles.connectButton, isLoading && styles.connectButtonDisabled]}
        onPress={onSubmit}
        disabled={isLoading}>
        {isLoading ? (
          <ActivityIndicator
            color="#fff"
            size="small"
          />
        ) : (
          <Text style={styles.connectButtonText}>Connect & Save</Text>
        )}
      </TouchableOpacity>
    </View>
  </View>
);

export default NewAccountForm;
