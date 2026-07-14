import { Modal, SafeAreaView, ScrollView, Text, TouchableOpacity, View } from 'react-native';

import { getPermissionDescription, permissionNames } from '../../utils/smbUtils';
import { aclStyles as styles } from './styles';

// Inner "Detailed Permissions" sheet shown from a permission row.

const PermissionDetailModal = ({ visible, permissions, sid, onClose }) => (
  <Modal
    visible={visible}
    animationType="slide"
    onRequestClose={onClose}>
    <SafeAreaView style={styles.detailModalContainer}>
      <View style={styles.detailModalHeader}>
        <Text style={styles.detailModalTitle}>Detailed Permissions</Text>
        {sid && <Text style={styles.detailModalSubtitle}>{sid}</Text>}
      </View>

      <ScrollView style={styles.detailModalContent}>
        {permissions.length > 0 ? (
          <View style={styles.allPermissionsContainer}>
            <Text style={styles.allPermissionsTitle}>All Granted Permissions:</Text>
            {permissions.map((permission, index) => {
              const displayName = permissionNames[permission] || permission;
              const description = getPermissionDescription(permission);
              return (
                <View
                  key={index}
                  style={styles.permissionDetailItem}>
                  <Text style={styles.permissionDetailName}>{'\u2022 ' + displayName}</Text>
                  {description && <Text style={styles.permissionDetailDescription}>{description}</Text>}
                </View>
              );
            })}
          </View>
        ) : (
          <Text style={styles.noPermissionsText}>No permissions granted</Text>
        )}
      </ScrollView>

      <TouchableOpacity
        style={styles.detailModalCloseButton}
        onPress={onClose}>
        <Text style={styles.detailModalCloseButtonText}>Close</Text>
      </TouchableOpacity>
    </SafeAreaView>
  </Modal>
);

export default PermissionDetailModal;
