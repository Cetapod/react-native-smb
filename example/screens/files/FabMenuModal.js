import { Modal, Text, TouchableOpacity, View } from 'react-native';

import { filesStyles as styles } from './styles';

// FAB-anchored "+" menu. onDismiss runs the deferred action so the picker
// can present after the modal animation completes.

const FabMenuModal = ({ visible, onClose, onDismiss, onPickNewFolder, onPickUpload }) => (
  <Modal
    visible={visible}
    transparent
    animationType="slide"
    onRequestClose={onClose}
    onDismiss={onDismiss}>
    <TouchableOpacity
      style={styles.fabMenuOverlay}
      activeOpacity={1}
      onPress={onClose}>
      <View style={[styles.bottomMenuModal, styles.fabMenuModal]}>
        <Text style={styles.menuTitle}>New</Text>
        <TouchableOpacity
          style={styles.menuItem}
          onPress={onPickNewFolder}>
          <Text style={styles.menuItemText}>New Folder</Text>
        </TouchableOpacity>
        <TouchableOpacity
          style={styles.menuItem}
          onPress={onPickUpload}>
          <Text style={styles.menuItemText}>Upload File</Text>
        </TouchableOpacity>
      </View>
    </TouchableOpacity>
  </Modal>
);

export default FabMenuModal;
