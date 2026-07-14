import { formatFileSize, formatDate, splitPath } from '@cetapod/react-native-smb';

// Re-export library utilities
export { formatFileSize, formatDate };

export const getFileIcon = (fileOrIsDirectory, fileName) => {
  let isDirectory = false;
  let fileNameStr = '';

  if (typeof fileOrIsDirectory === 'object' && fileOrIsDirectory !== null) {
    isDirectory = fileOrIsDirectory.isDirectory;
    fileNameStr = fileOrIsDirectory.name;
  } else if (typeof fileOrIsDirectory === 'boolean') {
    isDirectory = fileOrIsDirectory;
    fileNameStr = fileName;
  } else {
    fileNameStr = fileOrIsDirectory;
  }

  if (isDirectory) {
    return '📁';
  }

  const { extension } = splitPath(fileNameStr || '');
  const ext = extension.replace('.', '').toLowerCase();
  const iconMap = {
    txt: '📄',
    pdf: '📕',
    doc: '📘',
    docx: '📘',
    xls: '📗',
    xlsx: '📗',
    ppt: '📙',
    pptx: '📙',
    jpg: '🖼️',
    jpeg: '🖼️',
    png: '🖼️',
    gif: '🖼️',
    mp4: '🎬',
    avi: '🎬',
    mov: '🎬',
    mp3: '🎵',
    wav: '🎵',
    zip: '📦',
    rar: '📦',
    '7z': '📦',
  };
  return iconMap[ext] || '📄';
};

// Permission mask mappings for ACL permissions
export const permissionMasks = {
  Read: ['SYNCHRONIZE', 'READ_CONTROL', 'READ_ATTRIBUTES', 'READ_EA', 'READ_DATA'],
  Modify: [
    'SYNCHRONIZE',
    'READ_CONTROL',
    'DELETE',
    'WRITE_ATTRIBUTES',
    'READ_ATTRIBUTES',
    'WRITE_EA',
    'READ_EA',
    'APPEND_DATA',
    'WRITE_DATA',
    'READ_DATA',
  ],
  FullControl: [
    'SYNCHRONIZE',
    'WRITE_OWNER',
    'WRITE_DACL',
    'READ_CONTROL',
    'DELETE',
    'WRITE_ATTRIBUTES',
    'READ_ATTRIBUTES',
    'WRITE_EA',
    'READ_EA',
    'APPEND_DATA',
    'WRITE_DATA',
    'READ_DATA',
  ],
};

// Permission name mappings for display
export const permissionNames = {
  READ_DATA: 'Read Data',
  WRITE_DATA: 'Write Data',
  APPEND_DATA: 'Append Data',
  READ_EA: 'Read Extended Attributes',
  WRITE_EA: 'Write Extended Attributes',
  EXECUTE: 'Execute',
  DELETE_CHILD: 'Delete Child',
  READ_ATTRIBUTES: 'Read Attributes',
  WRITE_ATTRIBUTES: 'Write Attributes',
  DELETE: 'Delete',
  READ_CONTROL: 'Read Control',
  WRITE_DACL: 'Write DACL',
  WRITE_OWNER: 'Write Owner',
  SYNCHRONIZE: 'Synchronize',
};

// Permission descriptions for detailed view
export const permissionDescriptions = {
  READ_DATA: 'Allows reading the contents of files and folders',
  WRITE_DATA: 'Allows writing data to files and creating new files in folders',
  APPEND_DATA: 'Allows appending data to files and creating subfolders',
  READ_EA: 'Allows reading extended attributes of files and folders',
  WRITE_EA: 'Allows writing extended attributes to files and folders',
  EXECUTE: 'Allows executing files and traversing folders',
  DELETE_CHILD: 'Allows deleting files and folders within this folder',
  READ_ATTRIBUTES: 'Allows reading basic attributes (read-only, hidden, etc.)',
  WRITE_ATTRIBUTES: 'Allows changing basic attributes (read-only, hidden, etc.)',
  DELETE: 'Allows deleting the file or folder',
  READ_CONTROL: 'Allows reading security information (owner, group, permissions)',
  WRITE_DACL: 'Allows changing permissions on the file or folder',
  WRITE_OWNER: 'Allows taking ownership of the file or folder',
  SYNCHRONIZE: 'Allows using the file or folder for synchronization',
};

/**
 * Convert an ACE mask array to a human-readable permission level
 * @param {string[]} mask - Array of permission strings from ACE
 * @returns {string} - Permission level: "Read", "Modify", "FullControl", or "Custom"
 */
export const convertMaskToPermission = (mask) => {
  if (!Array.isArray(mask) || mask.length === 0) {
    return 'No Access';
  }

  // Sort both arrays to ensure consistent comparison
  const sortedMask = [...mask].sort();

  // Check for exact matches with predefined permission levels
  for (const [permissionLevel, requiredMask] of Object.entries(permissionMasks)) {
    const sortedRequiredMask = [...requiredMask].sort();

    if (
      sortedMask.length === sortedRequiredMask.length &&
      sortedMask.every((permission, index) => permission === sortedRequiredMask[index])
    ) {
      return permissionLevel;
    }
  }

  // Check if it's a subset of any predefined permission level
  const readMask = permissionMasks.Read;
  const modifyMask = permissionMasks.Modify;
  const fullControlMask = permissionMasks.FullControl;

  const hasAllReadPermissions = readMask.every((permission) => mask.includes(permission));
  const hasAllModifyPermissions = modifyMask.every((permission) => mask.includes(permission));
  const hasAllFullControlPermissions = fullControlMask.every((permission) => mask.includes(permission));

  if (hasAllFullControlPermissions) {
    return 'FullControl+';
  } else if (hasAllModifyPermissions) {
    return 'Modify+';
  } else if (hasAllReadPermissions) {
    return 'Read+';
  }

  return 'Custom';
};

/**
 * Check if a mask contains specific permission
 * @param {string[]} mask - Array of permission strings from ACE
 * @param {string} permission - Permission to check for
 * @returns {boolean} - Whether the permission is present
 */
export const hasPermission = (mask, permission) => {
  return Array.isArray(mask) && mask.includes(permission);
};

/**
 * Get permission description
 * @param {string} permission - Permission constant
 * @returns {string|null} - Permission description or null if not found
 */
export const getPermissionDescription = (permission) => {
  return permissionDescriptions[permission] || null;
};

/**
 * Check if a permission set matches a standard permission level
 * @param {string[]} permissions - Array of permission strings
 * @param {string} category - Permission category to check ('Read', 'Modify', 'FullControl')
 * @returns {boolean} - Whether permissions match the category
 */
export const hasPermissionCategory = (permissions, category) => {
  if (!Array.isArray(permissions) || !permissionMasks[category]) {
    return false;
  }
  return permissionMasks[category].every((perm) => permissions.includes(perm));
};

/**
 * Get simplified permission level from a permission set
 * @param {string[]} permissions - Array of permission strings
 * @returns {string} - Simplified permission level or "Custom"
 */
export const getSimplifiedPermission = (permissions) => {
  if (!permissions || !Array.isArray(permissions)) {
    return null;
  }

  if (hasPermissionCategory(permissions, 'FullControl')) {
    return 'Full Control';
  } else if (hasPermissionCategory(permissions, 'Modify')) {
    return 'Modify';
  } else if (hasPermissionCategory(permissions, 'Read')) {
    return 'Read';
  }
  return 'Custom';
};
