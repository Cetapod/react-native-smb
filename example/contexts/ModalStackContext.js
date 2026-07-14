import { createContext, useCallback, useContext, useMemo, useState } from 'react';

const ModalStackContext = createContext(null);

export const ModalStackProvider = ({ children }) => {
  const [selectedFile, setSelectedFile] = useState(null);
  const [showFileActionModal, setShowFileActionModal] = useState(false);
  const [showFileOperationModal, setShowFileOperationModal] = useState(false);
  const [showFolderPickerModal, setShowFolderPickerModal] = useState(false);
  const [fileOperationType, setFileOperationType] = useState('');

  const openFileAction = useCallback((file) => {
    setSelectedFile(file);
    setShowFileActionModal(true);
  }, []);

  const closeFileAction = useCallback(() => {
    setShowFileActionModal(false);
  }, []);

  const openFileOperation = useCallback((type) => {
    setFileOperationType(type);
    setShowFileOperationModal(true);
  }, []);

  const closeFileOperation = useCallback(() => {
    setShowFileOperationModal(false);
    setFileOperationType('');
    setSelectedFile(null);
  }, []);

  const openFolderPicker = useCallback(() => {
    setShowFolderPickerModal(true);
  }, []);

  const closeFolderPicker = useCallback(() => {
    setShowFolderPickerModal(false);
    setSelectedFile(null);
  }, []);

  const value = useMemo(
    () => ({
      selectedFile,
      setSelectedFile,
      showFileActionModal,
      showFileOperationModal,
      showFolderPickerModal,
      fileOperationType,
      openFileAction,
      closeFileAction,
      openFileOperation,
      closeFileOperation,
      openFolderPicker,
      closeFolderPicker,
    }),
    [selectedFile, showFileActionModal, showFileOperationModal, showFolderPickerModal, fileOperationType, openFileAction, closeFileAction, openFileOperation, closeFileOperation, openFolderPicker, closeFolderPicker],
  );

  return <ModalStackContext.Provider value={value}>{children}</ModalStackContext.Provider>;
};

export const useModalStack = () => {
  const ctx = useContext(ModalStackContext);
  if (!ctx) throw new Error('useModalStack must be used inside <ModalStackProvider>');
  return ctx;
};
