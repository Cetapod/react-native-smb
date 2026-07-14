import { Text, TouchableOpacity, View } from 'react-native';

import AccountItem from './AccountItem';
import { loginStyles as styles, MAX_VISIBLE_ACCOUNTS } from './styles';

// Saved-accounts list for the current server. Receives all data + handlers
// as props so it stays a pure presentation component.

const AccountsList = ({ accounts, showAllAccounts, setShowAllAccounts, isLoading, onSelect, onRemove, openRowRef, sectionTitle, onAddNewAccount }) => {
  const remaining = Math.max(0, accounts.length - MAX_VISIBLE_ACCOUNTS);
  const visible = accounts.slice(0, showAllAccounts ? undefined : MAX_VISIBLE_ACCOUNTS);

  return (
    <View style={styles.savedAccountsSection}>
      <View style={styles.sectionHeader}>
        <Text style={styles.sectionTitle}>{sectionTitle}</Text>
        {accounts.length > MAX_VISIBLE_ACCOUNTS && (
          <TouchableOpacity
            style={styles.headerToggleButton}
            onPress={() => setShowAllAccounts(!showAllAccounts)}
            activeOpacity={0.7}>
            <Text style={styles.headerToggleText}>{showAllAccounts ? 'Show Less' : `+${remaining} More`}</Text>
            <Text style={styles.headerToggleIcon}>{showAllAccounts ? '\u25B2' : '\u25BC'}</Text>
          </TouchableOpacity>
        )}
      </View>

      <View style={styles.accountsContainer}>
        {visible.map((account, index) => (
          <AccountItem
            key={index}
            account={account}
            index={index}
            isFirst={index === 0}
            isLoading={isLoading}
            onSelect={onSelect}
            onRemove={onRemove}
            openRowRef={openRowRef}
          />
        ))}
      </View>

      <TouchableOpacity
        style={styles.newAccountButton}
        onPress={onAddNewAccount}
        activeOpacity={0.7}>
        <Text style={styles.newAccountButtonText}>+ Add New Account</Text>
      </TouchableOpacity>
    </View>
  );
};

export default AccountsList;
