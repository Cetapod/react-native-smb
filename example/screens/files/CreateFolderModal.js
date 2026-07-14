import { Modal, Text, TextInput, TouchableOpacity, View } from 'react-native';

import { filesStyles as styles } from './styles';

const CreateFolderModal = ({ visible, folderName, setFolderName, onCancel, onSubmit }) => (
  <Modal
    visible={visible}
    transparent
    animationType="slide"
    onRequestClose={onCancel}>
    <View style={styles.modalOverlay}>
      <View style={styles.modalContent}>
        <Text style={styles.modalTitle}>New Folder</Text>
        <TextInput
          style={styles.modalInput}
          placeholder="Enter folder name"
          value={folderName}
          onChangeText={setFolderName}
          autoFocus={true}
        />
        <View style={styles.modalButtons}>
          <TouchableOpacity
            style={[styles.modalButton, styles.modalButtonCancel]}
            onPress={onCancel}>
            <Text style={styles.modalButtonText}>Cancel</Text>
          </TouchableOpacity>
          <TouchableOpacity
            style={[styles.modalButton, styles.modalButtonConfirm]}
            onPress={onSubmit}>
            <Text style={styles.modalButtonText}>Create</Text>
          </TouchableOpacity>
        </View>
      </View>
    </View>
  </Modal>
);

export default CreateFolderModal;
