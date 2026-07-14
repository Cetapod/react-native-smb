import { NavigationContainer } from '@react-navigation/native';
import { createNativeStackNavigator } from '@react-navigation/native-stack';
import { StatusBar } from 'expo-status-bar';
import { useState } from 'react';

import AclViewModal from './components/AclViewModal';
import CustomAlert from './components/CustomAlert';
import FileActionModal from './components/FileActionModal';
import FileInfoModal from './components/FileInfoModal';
import FileOperationModal from './components/FileOperationModal';
import FolderPickerModal from './components/FolderPickerModal';
import PoolDebugPanel from './components/PoolDebugPanel';
import TaskSnapshotBar from './components/TaskSnapshotBar';
import TransferTrayPanel from './components/TransferTrayPanel';
import { ModalStackProvider, useModalStack } from './contexts/ModalStackContext';
import { SmbProvider, useSmb } from './contexts/SmbContext';
import { useFileInfo } from './hooks/useFileInfo';
import { useFileOperations } from './hooks/useFileOperations';
import FilesScreen from './screens/FilesScreen';
import LoginScreen from './screens/LoginScreen';
import SharesScreen from './screens/SharesScreen';

const Stack = createNativeStackNavigator();

const AppShell = () => {
  const smb = useSmb();
  const modals = useModalStack();

  const fileOperations = useFileOperations(smb.smb, smb.currentPath, smb.loadFiles, smb.setIsLoading, smb.showAlert);
  const fileInfo = useFileInfo(smb.smb, smb.currentPath, smb.showAlert);

  const [pendingBulkMove, setPendingBulkMove] = useState(null);
  const [trackedDownloadTask, setTrackedDownloadTask] = useState(null);

  const handleAction = async (actionId, file) => {
    switch (actionId) {
      case 'info':
        await fileInfo.showFileInfo(file);
        break;
      case 'acl':
        await fileInfo.showFileAcl(file);
        break;
      case 'download':
        await fileOperations.downloadFile(file, { onTask: setTrackedDownloadTask });
        break;
      case 'copy':
        modals.openFileOperation('copy');
        break;
      case 'duplicate':
        await fileOperations.duplicateFile(file);
        break;
      case 'rename':
        modals.openFileOperation('rename');
        break;
      case 'move':
        modals.openFolderPicker();
        break;
      case 'delete':
        await fileOperations.deleteFile(file);
        break;
    }
  };

  const executeFileOperation = async (inputValue) => {
    await fileOperations.executeFileOperation(modals.selectedFile, modals.fileOperationType, inputValue);
  };

  const handleFolderSelect = async (destinationPath) => {
    if (pendingBulkMove) {
      const { items, onDone } = pendingBulkMove;
      setPendingBulkMove(null);
      modals.closeFolderPicker();
      if (typeof onDone === 'function') onDone();
      await fileOperations.moveFiles(items, destinationPath);
      return;
    }
    if (!modals.selectedFile) return;
    const destinationWithFile = destinationPath === '/' ? `/${modals.selectedFile.name}` : `${destinationPath}/${modals.selectedFile.name}`;
    await fileOperations.executeFileOperation(modals.selectedFile, 'move', destinationWithFile);
    modals.closeFolderPicker();
  };

  const FilesNavigator = () => {
    const FilesScreenComponent = ({ navigation }) => (
      <FilesScreen
        navigation={navigation}
        files={smb.filesList}
        currentPath={smb.currentPath}
        selectedShare={smb.selectedShare}
        username={smb.currentUsername}
        onLoadFiles={smb.loadFiles}
        onFileAction={modals.openFileAction}
        onFolderInfo={fileInfo.showCurrentFolderInfo}
        onCreateFolder={fileOperations.createFolder}
        onUploadFile={fileOperations.uploadFile}
        onRefresh={() => smb.loadFiles(smb.currentPath)}
        onBack={smb.handleBackToShares}
        onLogout={smb.handleBackToLogin}
        isLoading={smb.isLoading}
        showAlert={smb.showAlert}
        showPrompt={smb.showPrompt}
        onBulkDownload={(items) => fileOperations.downloadFiles(items, { autoShare: false })}
        onBulkDuplicate={(items) => fileOperations.duplicateFiles(items)}
        onBulkDelete={(items) => fileOperations.deleteFiles(items)}
        onBulkMove={(items, onDone) => {
          setPendingBulkMove({ items, onDone });
          modals.openFolderPicker();
        }}
        sortBy={smb.prefs.sortBy}
        sortOrder={smb.prefs.sortOrder}
        showHidden={smb.prefs.showHidden}
        onChangeSort={(sortBy, sortOrder) => smb.updatePrefs({ sortBy, sortOrder })}
        onToggleHidden={(showHidden) => smb.updatePrefs({ showHidden })}
      />
    );

    return (
      <Stack.Navigator screenOptions={{ headerShown: false, animation: 'slide_from_right' }}>
        <Stack.Screen
          name="FilesView"
          component={FilesScreenComponent}
        />
      </Stack.Navigator>
    );
  };

  const renderScreen = () => {
    switch (smb.currentScreen) {
      case 'login':
        return (
          <LoginScreen
            onLogin={smb.handleLogin}
            isLoading={smb.isLoading}
            showAlert={smb.showAlert}
          />
        );
      case 'shares':
        return (
          <SharesScreen
            shares={smb.sharesList}
            onSelectShare={smb.handleSelectShare}
            isLoading={smb.isLoading}
            onBack={smb.handleBackToLogin}
          />
        );
      case 'files':
        return (
          <NavigationContainer>
            <FilesNavigator />
          </NavigationContainer>
        );
      default:
        return null;
    }
  };

  if (smb.moduleError) {
    return (
      <LoginScreen
        onLogin={() => smb.showAlert('Error', smb.moduleError)}
        isLoading={false}
      />
    );
  }

  return (
    <>
      <StatusBar style="auto" />
      {renderScreen()}

      <FileActionModal
        visible={modals.showFileActionModal}
        file={modals.selectedFile}
        onClose={modals.closeFileAction}
        onAction={handleAction}
        showAlert={smb.showAlert}
      />

      <FileOperationModal
        visible={modals.showFileOperationModal}
        type={modals.fileOperationType}
        file={modals.selectedFile}
        onClose={modals.closeFileOperation}
        onExecute={executeFileOperation}
        showAlert={smb.showAlert}
      />

      <FolderPickerModal
        visible={modals.showFolderPickerModal}
        onClose={modals.closeFolderPicker}
        onSelect={handleFolderSelect}
        currentPath={smb.currentPath}
        SMB={smb.smb}
        initialShare={smb.selectedShare}
        showAlert={smb.showAlert}
        currentUsername={smb.currentUsername}
      />

      <FileInfoModal
        visible={fileInfo.showFileInfoModal}
        fileInfo={fileInfo.fileInfoData}
        onClose={fileInfo.closeFileInfoModal}
      />

      <AclViewModal
        visible={fileInfo.showAclModal}
        aclData={fileInfo.aclData}
        selectedFile={fileInfo.selectedFile?.name || modals.selectedFile?.name}
        loadingAcl={fileInfo.loadingAcl}
        onClose={fileInfo.closeAclModal}
      />

      <CustomAlert
        visible={smb.alertConfig.visible}
        type={smb.alertConfig.type}
        title={smb.alertConfig.title}
        message={smb.alertConfig.message}
        placeholder={smb.alertConfig.placeholder}
        buttons={smb.alertConfig.buttons}
        onClose={smb.hideAlert}
        onInputSubmit={smb.alertConfig.onInputSubmit}
      />

      {__DEV__ && <PoolDebugPanel />}
      <TaskSnapshotBar task={trackedDownloadTask} />
      <TransferTrayPanel />
    </>
  );
};

export default function App() {
  return (
    <SmbProvider>
      <ModalStackProvider>
        <AppShell />
      </ModalStackProvider>
    </SmbProvider>
  );
}
