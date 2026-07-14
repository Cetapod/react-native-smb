import { useState } from 'react';

import { buildFilePathWithoutLeadingSlash } from '../utils/connectionUtils';

export const useFileInfo = (SMB, currentPath, showAlert) => {
  const [showFileInfoModal, setShowFileInfoModal] = useState(false);
  const [fileInfoData, setFileInfoData] = useState(null);
  const [showAclModal, setShowAclModal] = useState(false);
  const [aclData, setAclData] = useState(null);
  const [loadingAcl, setLoadingAcl] = useState(false);
  const [selectedFile, setSelectedFile] = useState(null);

  const showFileInfo = async (file) => {
    try {
      const filePath = buildFilePathWithoutLeadingSlash(currentPath, file.name);
      const info = await SMB.getPathInfo(filePath).result();
      setFileInfoData(info);
      setShowFileInfoModal(true);
    } catch (error) {
      showAlert('Error', `Failed to get file info: ${error.message}`);
    }
  };

  const showCurrentFolderInfo = async () => {
    try {
      const folderPath = currentPath.startsWith('/') ? currentPath.substring(1) : currentPath;
      const info = await SMB.getPathInfo(folderPath || '.').result();
      setFileInfoData(info);
      setShowFileInfoModal(true);
    } catch (error) {
      showAlert('Error', `Failed to get folder info: ${error.message}`);
    }
  };

  const showFileAcl = async (file) => {
    try {
      setLoadingAcl(true);
      setShowAclModal(true);
      setSelectedFile(file);

      const filePath = buildFilePathWithoutLeadingSlash(currentPath, file.name);
      const acl = await SMB.getSecurityDescriptor(filePath).result();
      setAclData(acl);
    } catch (error) {
      showAlert('Error', `Failed to get permissions: ${error.message}`);
      setShowAclModal(false);
    } finally {
      setLoadingAcl(false);
    }
  };

  const closeFileInfoModal = () => {
    setShowFileInfoModal(false);
    setFileInfoData(null);
  };

  const closeAclModal = () => {
    setShowAclModal(false);
    setAclData(null);
    setSelectedFile(null);
  };

  return {
    showFileInfoModal,
    fileInfoData,
    showAclModal,
    aclData,
    loadingAcl,
    selectedFile,
    showFileInfo,
    showCurrentFolderInfo,
    showFileAcl,
    closeFileInfoModal,
    closeAclModal,
  };
};