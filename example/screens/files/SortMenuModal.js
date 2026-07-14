import { Modal, Switch, Text, TouchableOpacity, View } from 'react-native';

import { filesStyles as styles } from './styles';

const SortItem = ({ label, active, sortOrder, onPress }) => (
  <TouchableOpacity
    style={[styles.menuItem, active && styles.menuItemActive]}
    onPress={onPress}>
    <Text style={styles.menuItemText}>{label}</Text>
    {active && <Text style={styles.sortOrderIcon}>{sortOrder === 'asc' ? '\u2191' : '\u2193'}</Text>}
  </TouchableOpacity>
);

const SortMenuModal = ({ visible, onClose, sortBy, sortOrder, onSelectSort, username, selectedShare, onChangeAccount, showHidden, onToggleHidden }) => (
  <Modal
    visible={visible}
    transparent
    animationType="slide"
    onRequestClose={onClose}>
    <TouchableOpacity
      style={styles.menuOverlay}
      activeOpacity={1}
      onPress={onClose}>
      <View style={[styles.bottomMenuModal, styles.rightMenuModal]}>
        <Text style={styles.menuTitle}>Options</Text>

        {username && (
          <>
            <View style={styles.userInfoSection}>
              <Text style={styles.userInfoText}>Logged in as:</Text>
              <Text style={styles.userInfoUsername}>{username}</Text>
              <Text style={styles.userInfoShare}>@ {selectedShare}</Text>
            </View>
            <View style={styles.menuSeparator} />
          </>
        )}

        <Text style={styles.menuSectionTitle}>Sort Files</Text>
        <SortItem
          label="By Name"
          active={sortBy === 'name'}
          sortOrder={sortOrder}
          onPress={() => onSelectSort('name')}
        />
        <SortItem
          label="By Type"
          active={sortBy === 'type'}
          sortOrder={sortOrder}
          onPress={() => onSelectSort('type')}
        />
        <SortItem
          label="By Date"
          active={sortBy === 'date'}
          sortOrder={sortOrder}
          onPress={() => onSelectSort('date')}
        />
        <SortItem
          label="By Size"
          active={sortBy === 'size'}
          sortOrder={sortOrder}
          onPress={() => onSelectSort('size')}
        />

        <View style={styles.menuSeparator} />
        <Text style={styles.menuSectionTitle}>View</Text>
        <View style={[styles.menuItem, { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center' }]}>
          <Text style={styles.menuItemText}>Show hidden files</Text>
          <Switch
            value={!!showHidden}
            onValueChange={(v) => onToggleHidden && onToggleHidden(v)}
          />
        </View>

        <View style={styles.menuSeparator} />
        <Text style={styles.menuSectionTitle}>Account</Text>
        <TouchableOpacity
          style={styles.menuItem}
          onPress={onChangeAccount}>
          <Text style={styles.menuItemText}>Logout</Text>
        </TouchableOpacity>
      </View>
    </TouchableOpacity>
  </Modal>
);

export default SortMenuModal;
