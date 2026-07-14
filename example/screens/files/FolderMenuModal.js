import { Modal, Text, TouchableOpacity, View } from 'react-native';

import { filesStyles as styles } from './styles';

const FolderMenuModal = ({ visible, onClose, folderName, onShowFolderInfo, onRenamePrompt }) => (
  <Modal
    visible={visible}
    transparent
    animationType="slide"
    onRequestClose={onClose}>
    <TouchableOpacity
      style={styles.menuOverlay}
      activeOpacity={1}
      onPress={onClose}>
      <View style={[styles.bottomMenuModal, styles.centerMenuModal]}>
        <Text style={styles.menuTitle}>{folderName}</Text>
        <TouchableOpacity
          style={styles.menuItem}
          onPress={onShowFolderInfo}>
          <Text style={styles.menuItemText}>Folder Info</Text>
        </TouchableOpacity>
        <TouchableOpacity
          style={styles.menuItem}
          onPress={onRenamePrompt}>
          <Text style={styles.menuItemText}>Rename Folder</Text>
        </TouchableOpacity>
      </View>
    </TouchableOpacity>
  </Modal>
);

export default FolderMenuModal;
