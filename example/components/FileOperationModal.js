import React, { useState } from 'react';
import { Modal, StyleSheet, Text, TextInput, TouchableOpacity, View } from 'react-native';

const FileOperationModal = ({ visible, type, file, onClose, onExecute, showAlert }) => {
  const [inputValue, setInputValue] = useState('');

  const getFileExtension = (fileName) => {
    if (!fileName || !fileName.includes('.')) {
      return '';
    }
    const lastDotIndex = fileName.lastIndexOf('.');
    return fileName.substring(lastDotIndex);
  };

  const handleExecute = () => {
    if (!inputValue.trim()) {
      showAlert('Error', 'Please enter a value');
      return;
    }

    let finalValue = inputValue.trim();

    if ((type === 'rename' || type === 'copy') && file && !file.isDirectory) {
      const originalExtension = getFileExtension(file.name);
      if (originalExtension && !finalValue.endsWith(originalExtension)) {
        finalValue += originalExtension;
      }
    }

    if (type === 'move' && file) {
      if (!finalValue.endsWith('/')) {
        finalValue += '/';
      }
      finalValue += file.name;
    }

    onExecute(finalValue);
    setInputValue('');
    onClose();
  };

  const getFileNameWithoutExtension = (fileName) => {
    if (!fileName || !fileName.includes('.')) {
      return fileName;
    }
    const lastDotIndex = fileName.lastIndexOf('.');
    return fileName.substring(0, lastDotIndex);
  };

  const getCurrentPath = () => {
    if (!file?.path) {
      return '';
    }
    const pathParts = file.path.split('/');
    pathParts.pop();
    return pathParts.join('/') || '/';
  };

  const getModalConfig = () => {
    switch (type) {
      case 'rename':
        return {
          title: 'Rename',
          placeholder: 'Enter new name',
          buttonText: 'Rename',
          defaultValue: file?.name ? getFileNameWithoutExtension(file.name) : '',
        };
      case 'copy':
        return {
          title: 'Duplicate',
          placeholder: 'Enter copy name',
          buttonText: 'Create Copy',
          defaultValue: file?.name ? `${getFileNameWithoutExtension(file.name)} copy` : '',
        };
      case 'move':
        return {
          title: 'Move',
          placeholder: 'Enter destination path',
          buttonText: 'Move',
          defaultValue: getCurrentPath(),
        };
      default:
        return {
          title: 'File Operation',
          placeholder: 'Enter value',
          buttonText: 'Execute',
          defaultValue: '',
        };
    }
  };

  const config = getModalConfig();

  React.useEffect(() => {
    if (visible && config.defaultValue) {
      setInputValue(config.defaultValue);
    }
  }, [visible, config.defaultValue]);

  return (
    <Modal
      visible={visible}
      transparent={true}
      animationType="slide"
      onRequestClose={onClose}>
      <View style={styles.overlay}>
        <View style={styles.container}>
          <Text style={styles.title}>{config.title}</Text>

          {file && (
            <View style={styles.fileInfo}>
              <Text
                style={styles.fileName}
                numberOfLines={1}>
                {file.name}
              </Text>
              <Text style={styles.fileType}>{file.isDirectory ? 'Folder' : 'File'}</Text>
            </View>
          )}

          <View style={styles.inputContainer}>
            <TextInput
              style={[styles.input, file && !file.isDirectory && getFileExtension(file.name) && (type === 'rename' || type === 'copy') ? styles.inputWithExtension : null, file && type === 'move' ? styles.inputWithFilename : null]}
              placeholder={config.placeholder}
              value={inputValue}
              onChangeText={setInputValue}
              autoFocus={true}
              selectTextOnFocus={true}
            />
            {file && !file.isDirectory && getFileExtension(file.name) && (type === 'rename' || type === 'copy') && <Text style={styles.extensionText}>{getFileExtension(file.name)}</Text>}
            {file && type === 'move' && <Text style={styles.filenameText}>/{file.name}</Text>}
          </View>

          <View style={styles.buttons}>
            <TouchableOpacity
              style={[styles.button, styles.cancelButton]}
              onPress={onClose}>
              <Text style={styles.cancelButtonText}>Cancel</Text>
            </TouchableOpacity>
            <TouchableOpacity
              style={[styles.button, styles.confirmButton]}
              onPress={handleExecute}>
              <Text style={styles.confirmButtonText}>{config.buttonText}</Text>
            </TouchableOpacity>
          </View>
        </View>
      </View>
    </Modal>
  );
};

const styles = StyleSheet.create({
  overlay: {
    flex: 1,
    backgroundColor: 'rgba(128, 128, 128, 0.4)', // Neutral gray overlay
    justifyContent: 'center',
    alignItems: 'center',
  },
  container: {
    width: '85%',
    backgroundColor: '#F0F8FF',
    borderRadius: 12,
    padding: 20,
    borderWidth: 1,
    borderColor: '#87CEEB',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 6 },
    shadowOpacity: 0.15,
    shadowRadius: 12,
    elevation: 8,
  },
  title: {
    fontSize: 18,
    fontWeight: '600',
    color: '#191970',
    textAlign: 'center',
    marginBottom: 16,
  },
  fileInfo: {
    alignItems: 'center',
    justifyContent: 'center',
    marginBottom: 20,
    paddingHorizontal: 12,
    paddingVertical: 10,
    backgroundColor: '#E6F3FF',
    borderRadius: 8,
    borderWidth: 1,
    borderColor: '#B8D4F1',
  },
  fileIcon: {
    fontSize: 20,
    marginRight: 8,
  },
  fileName: {
    fontSize: 16,
    fontWeight: '600',
    color: '#191970',
    textAlign: 'center',
    marginBottom: 2,
  },
  fileType: {
    fontSize: 13,
    fontWeight: '400',
    color: '#4682B4',
    textAlign: 'center',
  },
  inputContainer: {
    position: 'relative',
    marginBottom: 20,
  },
  input: {
    height: 44,
    backgroundColor: '#FFFFFF',
    borderRadius: 8,
    paddingHorizontal: 12,
    fontSize: 16,
    borderWidth: 1,
    borderColor: '#87CEEB',
    color: '#191970',
  },
  inputWithExtension: {
    paddingRight: 60,
  },
  inputWithFilename: {
    paddingRight: 150,
  },
  extensionText: {
    position: 'absolute',
    right: 2,
    top: 2,
    bottom: 2,
    fontSize: 14,
    fontWeight: '500',
    color: '#6B7280',
    backgroundColor: '#F8FAFC',
    paddingHorizontal: 8,
    textAlignVertical: 'center',
    lineHeight: 40,
    borderTopRightRadius: 6,
    borderBottomRightRadius: 6,
    borderLeftWidth: 1,
    borderLeftColor: '#E2E8F0',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.05,
    shadowRadius: 2,
    elevation: 1,
    minWidth: 50,
  },
  filenameText: {
    position: 'absolute',
    right: 2,
    top: 2,
    bottom: 2,
    fontSize: 14,
    fontWeight: '500',
    color: '#6B7280',
    backgroundColor: '#F1F5F9',
    paddingHorizontal: 10,
    textAlignVertical: 'center',
    lineHeight: 40,
    borderTopRightRadius: 6,
    borderBottomRightRadius: 6,
    borderLeftWidth: 1,
    borderLeftColor: '#E2E8F0',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.05,
    shadowRadius: 2,
    elevation: 1,
  },
  buttons: {
    flexDirection: 'row',
    gap: 12,
  },
  button: {
    flex: 1,
    height: 44,
    borderRadius: 8,
    justifyContent: 'center',
    alignItems: 'center',
  },
  cancelButton: {
    backgroundColor: '#E6F3FF',
    borderWidth: 1,
    borderColor: '#87CEEB',
  },
  cancelButtonText: {
    fontSize: 16,
    fontWeight: '600',
    color: '#4682B4',
  },
  confirmButton: {
    backgroundColor: '#4169E1',
  },
  confirmButtonText: {
    fontSize: 16,
    fontWeight: '600',
    color: '#FFFFFF',
  },
});

export default FileOperationModal;
